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
#include<ctype.h>
#ifdef _WIN32
#include<windows.h>
#else
#include<dlfcn.h>
#endif

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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.car",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.car",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"sys.car",runnable,exe);
	fklNiReturn(obj->u.pair->car,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_set_car(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-car!",FKL_TOOMANYARG,runnable,exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-car!",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"sys.set-car!",runnable,exe);
	fklSetRef(obj,&obj->u.pair->car,target,exe->heap);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.cdr",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.cdr",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"sys.cdr",runnable,exe);
	fklNiReturn(obj->u.pair->cdr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_set_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-cdr!",FKL_TOOMANYARG,runnable,exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-cdr!",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"sys.set-cdr",runnable,exe);
	fklSetRef(obj,&obj->u.pair->cdr,target,exe->heap);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_cons(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.cons",FKL_TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.cons",FKL_TOOFEWARG,runnable,exe);
	fklNiReturn(fklNewVMpairV(car,cdr,stack,exe->heap),&ap,stack);
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
			FKL_NI_CHECK_TYPE(cur,FKL_IS_STR,"sys.append",runnable,exe);
			fklStringCat((FklString**)&(*prev)->u.str,cur->u.str);
		}
		else if(FKL_IS_VECTOR(*prev))
		{
			FKL_NI_CHECK_TYPE(cur,FKL_IS_VECTOR,"sys.append",runnable,exe);
			fklVMvecCat((FklVMvec**)&(*prev)->u.vec,cur->u.vec);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.append",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.eq",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.eq",FKL_TOOFEWARG,runnable,exe);
	fklNiReturn(fir==sec
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_equal(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.equal",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.equal",FKL_TOOFEWARG,runnable,exe);
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
	FklBigInt* bi=fklNewBigInt0();
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(fklIsFixint(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsI64AddOverflow(r64,c64))
				fklAddBigIntI(bi,c64);
			else
				r64+=fklGetInt(cur);
		}
		else if(FKL_IS_BIG_INT(cur))
			fklAddBigInt(bi,cur->u.bigInt);
		else if(FKL_IS_F64(cur))
			rd+=cur->u.f64;
		else
		{
			fklFreeBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.add",FKL_WRONGARG,runnable,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=0.0)
	{
		rd+=r64+fklBigIntToDouble(bi);
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		fklFreeBigInt(bi);
	}
	else if(FKL_IS_0_BIG_INT(bi))
	{
		fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
		fklFreeBigInt(bi);
	}
	else
	{
		fklAddBigIntI(bi,r64);
		if(fklIsGtLtI64BigInt(bi))
			fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),stack,exe->heap),&ap,stack);
			fklFreeBigInt(bi);
		}
	}
	fklNiEnd(&ap,stack);
}

void SYS_add_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.add_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"sys.add_1",runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&r,stack,exe->heap),&ap,stack);
	}
	else
	{
		if(FKL_IS_BIG_INT(arg))
		{
			if(fklIsGeLeI64BigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklNewBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklBigIntToI64(arg->u.bigInt)+1,stack,exe->heap),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==INT64_MAX)
			{
				FklBigInt* bi=fklNewBigInt(i);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklGetInt(arg)+1,stack,exe->heap),&ap,stack);
		}
	}
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.sub",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"sys.sub",runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else if(fklIsFixint(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64MulOverflow(p64,-1))
			{
				FklBigInt* bi=fklNewBigInt(p64);
				fklMulBigIntI(bi,-1);
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(-fklGetInt(prev),stack,exe->heap),&ap,stack);

		}
		else
		{
			FklBigInt* bi=fklNewBigInt0();
			fklSetBigInt(bi,prev->u.bigInt);
			fklMulBigIntI(bi,-1);
			if(fklIsGtLtI64BigInt(bi))
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			else
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),stack,exe->heap),&ap,stack);
				fklFreeBigInt(bi);
			}
		}
	}
	else
	{
		FklBigInt* bi=fklNewBigInt0();
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(fklIsFixint(cur))
			{
				int64_t c64=fklGetInt(cur);
				if(fklIsI64AddOverflow(r64,c64))
					fklAddBigIntI(bi,c64);
				else
					r64+=c64;
			}
			else if(FKL_IS_BIG_INT(cur))
				fklAddBigInt(bi,cur->u.bigInt);
			else if(FKL_IS_F64(cur))
				rd+=cur->u.f64;
			else
			{
				fklFreeBigInt(bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.sub",FKL_WRONGARG,runnable,exe);
			}
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=fklGetDouble(prev)-rd-r64-fklBigIntToDouble(bi);
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
			fklFreeBigInt(bi);
		}
		else if(FKL_IS_0_BIG_INT(bi)&&!FKL_IS_BIG_INT(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64AddOverflow(p64,-r64))
			{
				fklAddBigIntI(bi,p64);
				fklSubBigIntI(bi,r64);
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			}
			else
			{
				r64=p64-r64;
				fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
				fklFreeBigInt(bi);
			}
		}
		else
		{
			fklSubBigInt(bi,prev->u.bigInt);
			fklMulBigIntI(bi,-1);
			fklSubBigIntI(bi,r64);
			fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.sub_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"sys.add_1",runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&r,stack,exe->heap),&ap,stack);
	}
	else
	{
		if(FKL_IS_BIG_INT(arg))
		{
			if(fklIsGeLeI64BigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklNewBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklBigIntToI64(arg->u.bigInt)-1,stack,exe->heap),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==INT64_MIN)
			{
				FklBigInt* bi=fklNewBigInt(i);
				fklSubBigIntI(bi,1);
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklGetInt(arg)-1,stack,exe->heap),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void SYS_mul(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt* bi=fklNewBigInt1();
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(fklIsFixint(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsI64MulOverflow(r64,c64))
				fklMulBigIntI(bi,c64);
			else
				r64*=c64;
		}
		else if(FKL_IS_BIG_INT(cur))
			fklMulBigInt(bi,cur->u.bigInt);
		else if(FKL_IS_F64(cur))
			rd*=cur->u.f64;
		else
		{
			fklFreeBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.mul",FKL_WRONGARG,runnable,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=1.0)
	{
		rd*=r64*fklBigIntToDouble(bi);
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		fklFreeBigInt(bi);
	}
	else if(FKL_IS_1_BIG_INT(bi))
	{
		fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
		fklFreeBigInt(bi);
	}
	else
	{
		fklMulBigIntI(bi,r64);
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
	}
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.div",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"sys.div",runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=1/prev->u.f64;
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else
		{
			if(fklIsFixint(prev))
			{
				r64=fklGetInt(prev);
				if(!r64)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.div",FKL_DIVZERROERROR,runnable,exe);
				if(1%r64)
				{
					rd=1.0/r64;
					fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
				}
				else
					fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
			}
			else
			{
				if(FKL_IS_1_BIG_INT(prev->u.bigInt))
					fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
				else
				{
					double bd=fklBigIntToDouble(prev->u.bigInt);
					rd=1.0/bd;
					fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
				}
			}
		}
	}
	else
	{
		FklBigInt* bi=fklNewBigInt1();
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(fklIsFixint(cur))
			{
				int64_t c64=fklGetInt(cur);
				if(fklIsI64MulOverflow(r64,c64))
					fklMulBigIntI(bi,c64);
				else
					r64*=c64;
			}
			else if(FKL_IS_BIG_INT(cur))
				fklMulBigInt(bi,cur->u.bigInt);
			else if(FKL_IS_F64(cur))
				rd*=cur->u.f64;
			else
			{
				fklFreeBigInt(bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.div",FKL_WRONGARG,runnable,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi)||rd==0.0)
		{
			fklFreeBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.div",FKL_DIVZERROERROR,runnable,exe);
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)
				||rd!=1.0
				||(fklIsFixint(prev)
					&&FKL_IS_1_BIG_INT(bi)
					&&fklGetInt(prev)%(r64))
				||(FKL_IS_BIG_INT(prev)
					&&((!FKL_IS_1_BIG_INT(bi)&&!fklIsDivisibleBigInt(prev->u.bigInt,bi))
						||!fklIsDivisibleBigIntI(prev->u.bigInt,r64))))
		{
			rd=fklGetDouble(prev)/rd/r64/fklBigIntToDouble(bi);
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else
		{
			if(FKL_IS_BIG_INT(prev)&&!fklIsGtLtI64BigInt(prev->u.bigInt)&&!FKL_IS_1_BIG_INT(bi))
			{
				FklBigInt* t=fklNewBigInt0();
				fklSetBigInt(t,prev->u.bigInt);
				fklDivBigInt(t,bi);
				fklDivBigIntI(t,r64);
				if(fklIsGtLtI64BigInt(t))
					fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,t,stack,exe->heap),&ap,stack);
				else
				{
					fklNiReturn(fklMakeVMint(fklBigIntToI64(t),stack,exe->heap),&ap,stack);
					fklFreeBigInt(t);
				}
			}
			else
			{
				r64=fklGetInt(prev)/r64;
				fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
			}
		}
		fklFreeBigInt(bi);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=fklGetDouble(fir);
		double as=fklGetDouble(sec);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&r,stack,exe->heap),&ap,stack);
	}
	else if(fklIsFixint(fir)&&fklIsFixint(sec))
	{
		int64_t si=fklGetInt(sec);
		int64_t r=fklGetInt(fir)%si;
		if(si==0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
		fklNiReturn(fklMakeVMint(r,stack,exe->heap),&ap,stack);
	}
	else
	{
		FklBigInt* rem=fklNewBigInt0();
		if(FKL_IS_BIG_INT(fir))
			fklSetBigInt(rem,fir->u.bigInt);
		else
			fklSetBigIntI(rem,fklGetInt(fir));
		if(FKL_IS_BIG_INT(sec))
		{
			if(FKL_IS_0_BIG_INT(sec->u.bigInt))
			{
				fklFreeBigInt(rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
			}
			fklRemBigInt(rem,sec->u.bigInt);
		}
		else
		{
			int64_t si=fklGetInt(sec);
			if(si==0)
			{
				fklFreeBigInt(rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
			}
			fklRemBigIntI(rem,si);
		}
		if(fklIsGtLtI64BigInt(rem))
			fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,rem,stack,exe->heap),&ap,stack);
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(rem),stack,exe->heap),&ap,stack);
			fklFreeBigInt(rem);
		}
	}
	fklNiEnd(&ap,stack);
}

void SYS_eqn(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.eqn",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)&&fklIsVMnumber(cur))
					||(FKL_IS_F64(cur)&&fklIsVMnumber(prev)))
				r=fabs(fklGetDouble(prev)-fklGetDouble(cur))<DBL_EPSILON;
			else if(fklIsInt(prev)&&fklIsInt(cur))
			{
				if(fklIsFixint(prev)&&fklIsFixint(cur))
					r=fklGetInt(prev)==fklGetInt(cur);
				else
				{
					if(fklIsFixint(prev))
						r=fklCmpBigIntI(cur->u.bigInt,fklGetInt(prev))==0;
					else if(fklIsFixint(cur))
						r=fklCmpBigIntI(prev->u.bigInt,fklGetInt(cur))==0;
					else
						r=fklCmpBigInt(prev->u.bigInt,cur->u.bigInt)==0;
				}
			}
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklStringcmp(prev->u.str,cur->u.str)==0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)==0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.eqn",FKL_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)==0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.eqn",FKL_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.eqn",FKL_WRONGARG,runnable,exe);
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

void SYS_gt(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.gt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)&&fklIsVMnumber(cur))
					||(FKL_IS_F64(cur)&&fklIsVMnumber(prev)))
				r=(fklGetDouble(prev)-fklGetDouble(cur))>DBL_EPSILON;
			else if(fklIsInt(prev)&&fklIsInt(cur))
			{
				if(fklIsFixint(prev)&&fklIsFixint(cur))
					r=fklGetInt(prev)>fklGetInt(cur);
				else
				{
					if(fklIsFixint(prev))
						r=fklCmpBigIntI(cur->u.bigInt,fklGetInt(prev))<0;
					else if(fklIsFixint(cur))
						r=fklCmpBigIntI(prev->u.bigInt,fklGetInt(cur))>0;
					else
						r=fklCmpBigInt(prev->u.bigInt,cur->u.bigInt)>0;
				}
			}
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklStringcmp(prev->u.str,cur->u.str)>0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.gt",FKL_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.gt",FKL_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.gt",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.ge",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)&&fklIsVMnumber(cur))
					||(FKL_IS_F64(cur)&&fklIsVMnumber(prev)))
				r=(fklGetDouble(prev)-fklGetDouble(cur))>=DBL_EPSILON;
			else if(fklIsInt(prev)&&fklIsInt(cur))
			{
				if(fklIsFixint(prev)&&fklIsFixint(cur))
					r=fklGetInt(prev)>=fklGetInt(cur);
				else
				{
					if(fklIsFixint(prev))
						r=fklCmpBigIntI(cur->u.bigInt,fklGetInt(prev))<=0;
					else if(fklIsFixint(cur))
						r=fklCmpBigIntI(prev->u.bigInt,fklGetInt(cur))>=0;
					else
						r=fklCmpBigInt(prev->u.bigInt,cur->u.bigInt)>=0;
				}
			}
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklStringcmp(prev->u.str,cur->u.str)>=0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.ge",FKL_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.ge",FKL_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.ge",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.lt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)&&fklIsVMnumber(cur))
					||(FKL_IS_F64(cur)&&fklIsVMnumber(prev)))
				r=(fklGetDouble(prev)-fklGetDouble(cur))<-DBL_EPSILON;
			else if(fklIsInt(prev)&&fklIsInt(cur))
			{
				if(fklIsFixint(prev)&&fklIsFixint(cur))
					r=fklGetInt(prev)<fklGetInt(cur);
				else
				{
					if(fklIsFixint(prev))
						r=fklCmpBigIntI(cur->u.bigInt,fklGetInt(prev))>0;
					else if(fklIsFixint(cur))
						r=fklCmpBigIntI(prev->u.bigInt,fklGetInt(cur))<0;
					else
						r=fklCmpBigInt(prev->u.bigInt,cur->u.bigInt)<0;
				}
			}
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklStringcmp(prev->u.str,cur->u.str)<0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.lt",FKL_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.lt",FKL_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.lt",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.le",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)&&fklIsVMnumber(cur))
					||(FKL_IS_F64(cur)&&fklIsVMnumber(prev)))
				r=(fklGetDouble(prev)-fklGetDouble(cur))<=-DBL_EPSILON;
			else if(fklIsInt(prev)&&fklIsInt(cur))
			{
				if(fklIsFixint(prev)&&fklIsFixint(cur))
					r=fklGetInt(prev)<=fklGetInt(cur);
				else
				{
					if(fklIsFixint(prev))
						r=fklCmpBigIntI(cur->u.bigInt,fklGetInt(prev))>=0;
					else if(fklIsFixint(cur))
						r=fklCmpBigIntI(prev->u.bigInt,fklGetInt(cur))<=0;
					else
						r=fklCmpBigInt(prev->u.bigInt,cur->u.bigInt)<=0;
				}
			}
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklStringcmp(prev->u.str,cur->u.str)<=0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.le",FKL_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.le",FKL_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.le",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.char",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(obj))
	{
		FklVMvalue* pindex=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.char",FKL_TOOMANYARG,runnable,exe);
		if(pindex&&!fklIsInt(pindex))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.char",FKL_WRONGARG,runnable,exe);
		if(pindex)
		{
			uint64_t index=fklGetInt(pindex);
			if(index>=obj->u.str->size)
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
			fklNiReturn(FKL_MAKE_VM_CHR(obj->u.str->str[index]),&ap,stack);
		}
		else
			fklNiReturn(FKL_MAKE_VM_CHR(obj->u.str->str[0]),&ap,stack);
	}
	else
	{
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.char",FKL_TOOMANYARG,runnable,exe);
		if(fklIsInt(obj))
			fklNiReturn(FKL_MAKE_VM_CHR(fklGetInt(obj)),&ap,stack);
		else if(FKL_IS_CHR(obj))
			fklNiReturn(obj,&ap,stack);
		else if(FKL_IS_F64(obj))
			fklNiReturn(FKL_MAKE_VM_CHR((int32_t)obj->u.f64),&ap,stack);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.char",FKL_WRONGARG,runnable,exe);
	}
	fklNiEnd(&ap,stack);
}

void SYS_integer(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.integer",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.integer",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	else if(FKL_IS_I32(obj))
		fklNiReturn(obj,&ap,stack);
	else if(FKL_IS_SYM(obj))
		fklNiReturn(fklMakeVMint(FKL_GET_SYM(obj),stack,exe->heap),&ap,stack);
	else if(FKL_IS_I64(obj))
		fklNiReturn(fklMakeVMint(obj->u.i64,stack,exe->heap),&ap,stack);
	else if(FKL_IS_BIG_INT(obj))
	{
		if(fklIsGtLtI64BigInt(obj->u.bigInt))
		{
			FklBigInt* bi=fklNewBigInt0();
			fklSetBigInt(bi,obj->u.bigInt);
			fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
		}
		else
			fklNiReturn(fklMakeVMint(fklBigIntToI64(obj->u.bigInt),stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		fklNiReturn(fklMakeVMint(r,stack,exe->heap),&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.integer",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_to_int(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-int",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-int",FKL_TOOFEWARG,runnable,exe);
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
		if(!fklIsNumberString(obj->u.str))
			fklNiReturn(FKL_VM_NIL,&ap,stack);
		else
		{
			FklBigInt* bi=fklNewBigIntFromString(obj->u.str);
			if(!fklIsGtLtI64BigInt(bi))
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),stack,exe->heap),&ap,stack);
				fklFreeBigInt(bi);
			}
			else
				fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-int",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_to_f64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-f64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-f64",FKL_TOOFEWARG,runnable,exe);
	if(fklIsInt(obj)||FKL_IS_CHR(obj))
	{
		double r=fklIsInt(obj)?fklGetDouble(obj):FKL_GET_CHR(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_F64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_I64,(void*)&obj->u.i64,stack,exe->heap),&ap,stack);
	else if(FKL_IS_STR(obj))
	{
		if(!fklIsNumberString(obj->u.str))
			fklNiReturn(FKL_VM_NIL,&ap,stack);
		else
		{
			double d=fklStringToDouble(obj->u.str);
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&d,stack,exe->heap),&ap,stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-f64",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_i32(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.i32",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.i32",FKL_TOOFEWARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.i32",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_i64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.i64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.i64",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
	{
		int8_t r=FKL_GET_CHR(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(fklIsInt(obj))
	{
		int64_t r=fklGetInt(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_SYM(obj))
	{
		int64_t r=FKL_GET_SYM(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.i64",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_big_int(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.big-int",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.big-int",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,fklNewBigInt(FKL_GET_CHR(obj)),stack,exe->heap),&ap,stack);
	else if(FKL_IS_I32(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,fklNewBigInt(FKL_GET_I32(obj)),stack,exe->heap),&ap,stack);
	else if(FKL_IS_SYM(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,fklNewBigInt(FKL_GET_SYM(obj)),stack,exe->heap),&ap,stack);
	else if(FKL_IS_I64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,fklNewBigInt(obj->u.i64),stack,exe->heap),&ap,stack);
	else if(FKL_IS_F64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,fklNewBigInt(obj->u.f64),stack,exe->heap),&ap,stack);
	else if(FKL_IS_BIG_INT(obj))
	{
		FklBigInt* bi=fklNewBigInt0();
		fklSetBigInt(bi,obj->u.bigInt);
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_STR(obj))
	{
		FklBigInt* bi=fklNewBigIntFromMem(obj->u.str->str,obj->u.str->size);
		if(!bi)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.big-int",FKL_FAILD_TO_CREATE_BIG_INT_FROM_MEM,runnable,exe);
		fklNiReturn(fklNiNewVMvalue(FKL_BIG_INT,bi,stack,exe->heap),&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.big-int",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_f64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.f64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.f64",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNiNewVMvalue(FKL_F64,NULL,stack,exe->heap);
	if(FKL_IS_CHR(obj))
		retval->u.f64=(double)FKL_GET_CHR(obj);
	else if(fklIsInt(obj))
		retval->u.f64=(double)fklGetInt(obj);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.f64",FKL_WRONGARG,runnable,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_as_str(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.as-str",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNiNewVMvalue(FKL_STR,NULL,stack,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklNiGetArg(&ap,stack);
		FklVMvalue* psize=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.as-str",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			FKL_NI_CHECK_TYPE(pstart,fklIsInt,"sys.as-str",runnable,exe);
			int64_t start=fklGetInt(pstart);
			if((FKL_IS_STR(obj)&&(start<0||start>=obj->u.str->size))
					||(FKL_IS_VECTOR(obj)&&(start<0||start>=obj->u.vec->size)))
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.as-str",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_STR(obj)?obj->u.str->size-start:obj->u.vec->size-start;
			if(psize)
			{
				FKL_NI_CHECK_TYPE(psize,fklIsInt,"sys.as-str",runnable,exe);
				int64_t tsize=fklGetInt(psize);
				if(tsize<0||tsize>size)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.as-str",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			if(FKL_IS_STR(obj))
				retval->u.str=fklNewString(size,obj->u.str->str+start);
			else
			{
				retval->u.str=fklNewString(size,NULL);
				for(size_t i=0;i<size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[start+i];
					FKL_NI_CHECK_TYPE(v,fklIsInt,"sys.as-str",runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
		else
		{
			if(FKL_IS_STR(obj))
				retval->u.str=fklCopyString(obj->u.str);
			else
			{
				retval->u.str=fklNewString(obj->u.vec->size,NULL);
				FKL_ASSERT(retval->u.str,__func__);
				for(size_t i=0;i<obj->u.vec->size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[i];
					FKL_NI_CHECK_TYPE(v,fklIsInt,"sys.as-str",runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
	}
	else if(FKL_IS_CHR(obj))
	{
		retval->u.str=fklNewEmptyString();
		retval->u.str->size=0;
		for(size_t i=0;obj;i++,obj=fklNiGetArg(&ap,stack))
		{
			FKL_NI_CHECK_TYPE(obj,FKL_IS_CHR,"sys.as-str",runnable,exe);
			retval->u.str->size++;
			retval->u.str=(FklString*)realloc(retval->u.str
					,sizeof(FklString)+sizeof(char)*(i+1));
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
			FKL_NI_CHECK_TYPE(content,FKL_IS_CHR,"sys.as-str",runnable,exe);
			size_t size=fklGetInt(obj);
			retval->u.str=fklNewString(size,NULL);
			FKL_ASSERT(retval->u.str,__func__);
			memset(retval->u.str->str,FKL_GET_CHR(content),size);
		}
		else
		{
			if(FKL_IS_I32(obj))
			{
				int32_t r=FKL_GET_I32(obj);
				retval->u.str=fklNewString(sizeof(int32_t),(char*)&r);
			}
			else if(FKL_IS_I64(obj))
				retval->u.str=fklNewString(sizeof(int64_t),(char*)&obj->u.i64);
			else
			{
				FklBigInt* bi=obj->u.bigInt;
				retval->u.str=fklNewString(bi->num+1,NULL);
				retval->u.str->str[0]=bi->neg;
				memcpy(retval->u.str->str+1,bi->digits,bi->num);
			}
		}
	}
	else
	{
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.as-str",FKL_TOOMANYARG,runnable,exe);
		else if(FKL_IS_F64(obj))
			retval->u.str=fklNewString(sizeof(double),(char*)&obj->u.f64);
		else if(FKL_IS_SYM(obj))
		{
			FklSid_t sid=FKL_GET_SYM(obj);
			retval->u.str=fklNewString(sizeof(FklSid_t),(char*)&sid);
		}
		else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__as_str)
			retval->u.str=obj->u.ud->t->__as_str(obj->u.ud->data);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.as-str",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.symbol",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.symbol",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbolToGlobCstr(((char[]){FKL_GET_CHR(obj),'\0'}))->id),&ap,stack);
	else if(FKL_IS_SYM(obj))
		fklNiReturn(obj,&ap,stack);
	else if(FKL_IS_STR(obj))
		fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbolToGlob(obj->u.str)->id),&ap,stack);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.symbol",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nth",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nth",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"sys.nth",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nth",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
			fklNiReturn(objPair->u.pair->car,&ap,stack);
		else
			fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nth",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_set_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nth!",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nth!",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"sys.set-nth!",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nth!",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			fklSetRef(objPair,&objPair->u.pair->car,target,exe->heap);
			fklNiReturn(target,&ap,stack);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nth!",FKL_INVALIDASSIGN,runnable,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nth!",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_TOOMANYARG,runnable,exe);
	if(!place||!vector)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||(!FKL_IS_VECTOR(vector)&&!FKL_IS_STR(vector)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=FKL_IS_STR(vector)?vector->u.str->size:vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
	if(FKL_IS_STR(vector))
	{
		if(index>=vector->u.str->size)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
		fklNiReturn(FKL_MAKE_VM_CHR(vector->u.str->str[index]),&ap,stack);
	}
	else if(FKL_IS_VECTOR(vector))
	{
		if(index>=vector->u.vec->size)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
		fklNiReturn(vector->u.vec->base[index],&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void SYS_set_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_TOOMANYARG,runnable,exe);
	if(!place||!vector||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||(!FKL_IS_VECTOR(vector)&&!FKL_IS_STR(vector)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=FKL_IS_STR(vector)?vector->u.str->size:vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_INVALIDACCESS,runnable,exe);
	if(FKL_IS_STR(vector))
	{
		if(index>=vector->u.str->size)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_INVALIDACCESS,runnable,exe);
		if(!FKL_IS_CHR(target)&&!fklIsInt(target))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_WRONGARG,runnable,exe);
		vector->u.str->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
		fklNiReturn(target,&ap,stack);
	}
	else if(FKL_IS_VECTOR(vector))
	{
		if(index>=vector->u.vec->size)
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-vref!",FKL_INVALIDACCESS,runnable,exe);
		fklSetRef(vector,&vector->u.vec->base[index],target,exe->heap);
		fklNiReturn(target,&ap,stack);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nthcdr",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nthcdr",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"sys.nthcdr",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nthcdr",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
			fklNiReturn(objPair->u.pair->cdr,&ap,stack);
		else
			fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.nthcdr",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_set_nthcdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nthcdr!",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nthcdr!",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"sys.set-nthcdr!",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nthcdr!",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			fklSetRef(objPair,&objPair->u.pair->cdr,target,exe->heap);
			fklNiReturn(target,&ap,stack);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nthcdr!",FKL_INVALIDACCESS,runnable,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-nthcdr!",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_length(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.length",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.length",FKL_TOOFEWARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.length",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fopen",FKL_TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fopen",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fopen",FKL_WRONGARG,runnable,exe);
	char* c_filename=fklStringToCstr(filename->u.str);
	char* c_mode=fklStringToCstr(mode->u.str);
	FILE* file=fopen(c_filename,c_mode);
	free(c_mode);
	FklVMvalue* obj=NULL;
	if(!file)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("sys.fopen",c_filename,1,FKL_FILEFAILURE,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fclose",FKL_TOOFEWARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fclose",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"sys.fclose",runnable,exe);
	if(fp->u.fp==NULL||fklFreeVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fclose",FKL_INVALIDACCESS,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.read",FKL_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream)&&!FKL_IS_STR(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.read",FKL_WRONGARG,runnable,exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.read",FKL_UNEXPECTEOF,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fgetb",FKL_TOOMANYARG,runnable,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fgetb",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fgetb",FKL_WRONGARG,runnable,exe);
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
		vmstr->u.str=(FklString*)malloc(sizeof(FklString)+fklGetInt(psize));
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.prin1",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.prin1",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.prin1",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.princ",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.princ",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.princ",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.dlopen",FKL_TOOMANYARG,runnable,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.dlopen",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(dllName,FKL_IS_STR,"sys.dlopen",runnable,exe);
	char* str=fklStringToCstr(dllName->u.str);
	FklVMdllHandle* dll=fklNewVMdll(str);
	if(!dll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("sys.dlopen",str,1,FKL_LOADDLLFAILD,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.dlsym",FKL_TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.dlsym",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(dll))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.dlsym",FKL_WRONGARG,runnable,exe);
	if(!dll->u.dll)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.dlsym",FKL_INVALIDACCESS,runnable,exe);
	char prefix[]="FKL_";
	char* str=fklStringToCstr(symbol->u.str);
	size_t len=strlen(prefix)+strlen(str)+1;
	char* realDlFuncName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDlFuncName,__func__);
	sprintf(realDlFuncName,"%s%s",prefix,str);
	FklVMdllFunc funcAddress=fklGetAddress(realDlFuncName,dll->u.dll);
	free(realDlFuncName);
	if(!funcAddress)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("sys.dlsym",str,1,FKL_INVALIDSYMBOL,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.argv",FKL_TOOMANYARG,exe->rhead,exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&(*tmp)->u.pair->cdr)
		*tmp=fklNewVMpairV(fklNiNewVMvalue(FKL_STR,fklNewString(strlen(argv[i]),argv[i]),stack,exe->heap),FKL_VM_NIL,stack,exe->heap);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_go(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(exe->VMid==-1)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.go",FKL_CANTCREATETHREAD,runnable,exe);
	FklVMvalue* threadProc=fklNiGetArg(&ap,stack);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.go",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!FKL_IS_CONT(threadProc)&&!fklIsInvokableUd(threadProc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.go",FKL_WRONGARG,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.chanl",FKL_TOOMANYARG,runnable,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.chanl",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(maxSize,fklIsInt,"sys.chanl",runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.send",FKL_TOOMANYARG,runnable,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.send",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"sys.send",runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.recv",FKL_TOOMANYARG,runnable,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.recv",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"sys.recv",runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.error",FKL_TOOMANYARG,runnable,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.error",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.error",FKL_WRONGARG,runnable,exe);
	fklNiReturn(fklNiNewVMvalue(FKL_ERR,fklNewVMerror((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:who->u.str,FKL_GET_SYM(type),message->u.str),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_raise(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* err=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.raise",FKL_TOOMANYARG,runnable,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.raise",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(err,FKL_IS_ERR,"sys.raise",runnable,exe);
	fklNiEnd(&ap,stack);
	fklRaiseVMerror(err,exe);
}

void SYS_call_cc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.call/cc",FKL_TOOMANYARG,runnable,exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.call/cc",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PTR(proc)||(proc->type!=FKL_PROC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.call/cc",FKL_INVOKEERROR,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.apply",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(proc)&&!FKL_IS_CONT(proc)&&!FKL_IS_DLPROC(proc)&&!fklIsInvokableUd(proc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.invoke",FKL_INVOKEERROR,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.apply",FKL_WRONGARG,runnable,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=fklGetVMpairCdr(lastList))
		fklPushPtrStack(fklGetVMpairCar(lastList),stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklFreePtrStack(stack1);
		fklFreePtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.apply",FKL_WRONGARG,runnable,exe);
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

void SYS_map(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	fklNiEnd(&ap,stack);
}

void SYS_reverse(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.reverse",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.reverse",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj)&&obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.reverse",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(obj!=FKL_VM_NIL)
	{
		FklVMheap* heap=exe->heap;
		for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=cdr->u.pair->cdr)
			retval=fklNewVMpairV(cdr->u.pair->car,retval,stack,heap);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.feof",FKL_TOOMANYARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.feof",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"sys.feof",runnable,exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.feof",FKL_INVALIDACCESS,runnable,exe);
	fklNiReturn(!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	if(!fir)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(fir)||FKL_IS_VECTOR(fir))
	{
		FklVMvalue* startV=fklNiGetArg(&ap,stack);
		FklVMvalue* sizeV=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_TOOMANYARG,runnable,exe);
		if((startV&&!fklIsInt(startV))
				||(sizeV&&!fklIsInt(sizeV)))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_WRONGARG,runnable,exe);
		int64_t start=0;
		size_t size=FKL_IS_STR(fir)?fir->u.str->size:fir->u.vec->size;
		if(startV)
		{
			int64_t tstart=fklGetInt(startV);
			if(tstart<0||tstart>size)
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_INVALIDACCESS,runnable,exe);
			start=tstart;
		}
		size=size-start;
		if(sizeV)
		{
			int64_t tsize=fklGetInt(sizeV);
			if(tsize<0||tsize>size)
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_INVALIDACCESS,runnable,exe);
			size=tsize;
		}
		FklVMvec* vec=fklNewVMvec(size);
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
			FklVMvalue* vec=fklNewVMvecV(vstack->top,(FklVMvalue**)vstack->base,stack,exe->heap);
			fklFreePtrStack(vstack);
			fklNiReturn(vec,&ap,stack);
		}
		else
		{
			FklVMvalue* content=fklNiGetArg(&ap,stack);
			if(fklNiResBp(&ap,stack))
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_TOOMANYARG,runnable,exe);
			FklVMvalue* vec=fklNewVMvecV(size,NULL,stack,exe->heap);
			if(content)
				for(size_t i=0;i<size;i++)
					fklSetRef(vec,&vec->u.vec->base[i],content,exe->heap);
			else
				for(size_t i=0;i<size;i++)
					vec->u.vec->base[i]=FKL_VM_NIL;
			fklNiReturn(vec,&ap,stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.vector",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_getdir(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.getdir",FKL_TOOMANYARG,exe->rhead,exe);
	FklLineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
	if(node->fid)
	{
		char* filename=fklCopyCstr(fklGetMainFileRealPath());
		filename=fklStrCat(filename,FKL_PATH_SEPARATOR_STR);
		filename=fklCstrStringCat(filename,fklGetGlobSymbolWithId(node->fid)->symbol);
		char* rpath=fklRealpath(filename);
		char* dir=fklGetDir(rpath);
		free(filename);
		free(rpath);
		FklString* str=fklNewString(strlen(dir),dir);
		free(dir);
		fklNiReturn(fklNiNewVMvalue(FKL_STR,str,stack,exe->heap),&ap,stack);
	}
	else
	{
		const char* cwd=fklGetCwd();
		FklString* str=fklNewString(strlen(cwd),cwd);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fgetc",FKL_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fgetc",FKL_WRONGARG,runnable,exe);
	FklVMfp* fp=stream?stream->u.fp:fklGetVMstdin()->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fgetc",FKL_INVALIDACCESS,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fwrite",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fwrite",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fwrite",FKL_WRONGARG,runnable,exe);
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
	else if(FKL_IS_BIG_INT(obj))
	{
		uint64_t len=obj->u.bigInt->num+1;
		fwrite(&len,sizeof(len),1,objFile);
		fwrite(&obj->u.bigInt->neg,sizeof(uint8_t),1,objFile);
		fwrite(obj->u.bigInt->digits,sizeof(uint8_t),obj->u.bigInt->num,objFile);
	}
	else if(FKL_IS_I64(obj))
		fwrite((void*)&obj->u.i64,sizeof(int64_t),1,objFile);
	else if(FKL_IS_F64(obj))
		fwrite((void*)&obj->u.f64,sizeof(double),1,objFile);
	else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__write)
		obj->u.ud->t->__write(objFile,obj->u.ud->data);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.fwrite",FKL_WRONGARG,runnable,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_to_str(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-str",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNiNewVMvalue(FKL_STR,NULL,stack,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklNiGetArg(&ap,stack);
		FklVMvalue* psize=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-str",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			FKL_NI_CHECK_TYPE(pstart,fklIsInt,"sys.to-str",runnable,exe);
			int64_t start=fklGetInt(pstart);
			if((FKL_IS_STR(obj)&&start>=obj->u.str->size)
					||(FKL_IS_VECTOR(obj)&&start>=obj->u.vec->size))
				FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-str",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_STR(obj)?obj->u.str->size-start:obj->u.vec->size-start;
			if(psize)
			{
				FKL_NI_CHECK_TYPE(psize,fklIsInt,"sys.to-str",runnable,exe);
				int64_t tsize=fklGetInt(psize);
				if(tsize>size)
					FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-str",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			if(FKL_IS_STR(obj))
				retval->u.str=fklNewString(size,obj->u.str->str+start);
			else
			{
				retval->u.str=fklNewString(size,NULL);
				for(size_t i=0;i<size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[start+i];
					FKL_NI_CHECK_TYPE(v,fklIsInt,"sys.to-str",runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
		else
		{
			if(FKL_IS_STR(obj))
				retval->u.str=fklCopyString(obj->u.str);
			else
			{
				retval->u.str=fklNewString(obj->u.vec->size,NULL);
				FKL_ASSERT(retval->u.str,__func__);
				for(size_t i=0;i<obj->u.vec->size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[i];
					FKL_NI_CHECK_TYPE(v,fklIsInt,"sys.to-str",runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
	}
	else if(FKL_IS_CHR(obj))
	{
		retval->u.str=fklNewEmptyString();
		retval->u.str->size=0;
		for(size_t i=0;obj;i++,obj=fklNiGetArg(&ap,stack))
		{
			FKL_NI_CHECK_TYPE(obj,FKL_IS_CHR,"sys.to-str",runnable,exe);
			retval->u.str->size++;
			retval->u.str=(FklString*)realloc(retval->u.str
					,sizeof(FklString)+sizeof(char)*(i+1));
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
			FKL_NI_CHECK_TYPE(content,FKL_IS_CHR,"sys.to-str",runnable,exe);
			size_t size=fklGetInt(obj);
			retval->u.str=fklNewString(size,NULL);
			FKL_ASSERT(retval->u.str,__func__);
			memset(retval->u.str->str,FKL_GET_CHR(content),size);
		}
		else
		{
			if(FKL_IS_BIG_INT(obj))
			{
				FklBigInt* bi=obj->u.bigInt;
				retval->u.str=fklNewString(sizeof(char)*(bi->num+bi->neg),NULL);
				fklSprintBigInt(bi,retval->u.str->size,retval->u.str->str);
			}
			else
			{
				char buf[32]={0};
				size_t size=snprintf(buf,32,"%ld",fklGetInt(obj));
				retval->u.str=fklNewString(size,buf);
			}
		}
	}
	else
	{
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-str",FKL_TOOMANYARG,runnable,exe);
		else if(FKL_IS_F64(obj))
		{
			char buf[64]={0};
			size_t size=snprintf(buf,64,"%lf",obj->u.f64);
			retval->u.str=fklNewString(size,buf);
		}
		else if(FKL_IS_SYM(obj))
		{
			FklString* symbol=fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol;
			retval->u.str=fklCopyString(symbol);
		}
		else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__to_str)
			retval->u.str=obj->u.ud->t->__to_str(obj->u.ud->data);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("sys.to-str",FKL_WRONGARG,runnable,exe);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.box",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.box",FKL_TOOFEWARG,runnable,exe);
	fklNiReturn(fklNiNewVMvalue(FKL_BOX,obj,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_unbox(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.unbox",FKL_TOOMANYARG,runnable,exe);
	if(!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.unbox",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"sys.unbox",runnable,exe);
	fklNiReturn(box->u.box,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_set_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-box!",FKL_TOOMANYARG,runnable,exe);
	if(!obj||!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.set-box!",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"sys.set-box!",runnable,exe);
	fklSetRef(box,&box->u.box,obj,exe->heap);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_box_cas(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* new=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.box-cas!",FKL_TOOMANYARG,runnable,exe);
	if(!old||!box||!new)
		FKL_RAISE_BUILTIN_ERROR_CSTR("sys.box-cas!",FKL_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"sys.box-cas!",runnable,exe);
	if(box->u.box==old)
	{
		fklSetRef(box,&box->u.box,new,exe->heap);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

#define PREDICATE(condtion,err_infor) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
		FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_TOOMANYARG,runnable,exe);\
	if(!val)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_TOOFEWARG,runnable,exe);\
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
void SYS_integer_p(ARGL) PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val)||FKL_IS_BIG_INT(val),"sys.integer?")
void SYS_fix_int_p(ARGL) PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val),"sys.fix-int?")
void SYS_i32_p(ARGL) PREDICATE(FKL_IS_I32(val),"sys.i32?")
void SYS_i64_p(ARGL) PREDICATE(FKL_IS_I64(val),"sys.i64?")
void SYS_f64_p(ARGL) PREDICATE(FKL_IS_F64(val),"sys.i64?")
void SYS_number_p(ARGL) PREDICATE(fklIsVMnumber(val),"sys.number?")
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
void SYS_big_int_p(ARGL) PREDICATE(FKL_IS_BIG_INT(val),"sys.big-int?")
void SYS_list_p(ARGL) PREDICATE(fklIsList(val),"sys.list?")
void SYS_box_p(ARGL) PREDICATE(FKL_IS_BOX(val),"sys.box?")

#undef ARGL
#undef PREDICATE
//end
