#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/builtin.h>
#ifdef _WIN32
#include<conio.h>
#include<windows.h>
#else
#include<termios.h>
#include<unistd.h>
#endif
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

static FklVMvalue* BuiltInStdin=NULL;
static FklVMvalue* BuiltInStdout=NULL;
static FklVMvalue* BuiltInStderr=NULL;


static const char* builtInErrorType[]=
{
	"dummy",
	"symbol-undefined",
	"syntax-error",
	"invalid-expression",
	"circular-load",
	"invalid-pattern",
	"wrong-types-of-arguements",
	"stack-error",
	"too-many-arguements",
	"too-few-arguements",
	"cant-create-threads",
	"thread-error",
	"macro-expand-error",
	"call-error",
	"load-dll-faild",
	"invalid-symbol",
	"library-undefined",
	"unexpect-eof",
	"div-zero-error",
	"file-failure",
	"invalid-member-symbol",
	"invalid-assign",
	"invalid-access",
	"import-failed",
	"invalid-macro-pattern",
	"faild-to-create-big-int-from-mem",
	"differ-list-in-list",
	"cross-c-call-continuation",
	"invalid-radix",
	NULL,
};

FklSid_t fklGetBuiltInErrorType(FklBuiltInErrorType type)
{
	static FklSid_t errorTypeId[sizeof(builtInErrorType)/sizeof(const char*)]={0};
	if(!errorTypeId[type])
		errorTypeId[type]=fklAddSymbolToGlobCstr(builtInErrorType[type])->id;
	return errorTypeId[type];
}

extern void applyNativeProc(FklVM*,FklVMproc*,FklVMrunnable*);
extern void* ThreadVMfunc(void* p);

//builtin functions

#define ARGL FklVM* exe
#define K_FUNC_ARGL FklVM* exe,FklCCState s,void* ctx
void builtin_car(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.car",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.car",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.car",runnable,exe);
	fklNiReturn(obj->u.pair->car,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_car(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-car!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-car!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.set-car!",runnable,exe);
	fklSetRef(&obj->u.pair->car,target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cdr",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cdr",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.cdr",runnable,exe);
	fklNiReturn(obj->u.pair->cdr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-cdr!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-cdr!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.set-cdr",runnable,exe);
	fklSetRef(&obj->u.pair->cdr,target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cons(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cons",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cons",FKL_ERR_TOOFEWARG,runnable,exe);
	fklNiReturn(fklNewVMpairV(car,cdr,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

static int __fkl_str_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_STR(cur))
		return 1;
	fklStringCat(&retval->u.str,cur->u.str);
	return 0;
}

static int __fkl_vector_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_VECTOR(cur))
		return 1;
	fklVMvecCat((FklVMvec**)&retval->u.vec,cur->u.vec);
	return 0;
}

static int __fkl_bytevector_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_BYTEVECTOR(cur))
		return 1;
	fklBytevectorCat(&retval->u.bvec,cur->u.bvec);
	return 0;
}

static int __fkl_userdata_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_USERDATA(cur)
			||retval->u.ud->type!=cur->u.ud->type
			||!retval->u.ud->t->__append
			||retval->u.ud->t->__append!=cur->u.ud->t->__append)
		return 1;
	cur->u.ud->t->__append(&retval->u.ud->data,cur->u.ud->data);
	return 0;
}

static int (*const valueAppend[])(FklVMvalue* retval,FklVMvalue* cur)=
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	__fkl_str_append,
	__fkl_vector_append,
	NULL,
	NULL,
	__fkl_bytevector_append,
	__fkl_userdata_append,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

void builtin_copy(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_TOOMANYARG,runnable,exe);
	FklVMvalue* retval=fklCopyVMvalue(obj,stack,exe->gc);
	if(!retval)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_WRONGARG,runnable,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_append(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=fklNiGetArg(&ap,stack);
		if(cur)
			retval=fklCopyVMvalue(retval,stack,exe->gc);
		if(!retval)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_WRONGARG,runnable,exe);
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_WRONGARG,runnable,exe);
		}
	}
	else
	{
		FklVMvalue** prev=&retval;
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(*prev==FKL_VM_NIL)
			{
				*prev=cur;
				*prev=fklCopyVMlistOrAtom(*prev,stack,exe->gc);
				for(;FKL_IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_WRONGARG,runnable,exe);
		}
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_eq(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOFEWARG,runnable,exe);
	fklNiReturn(fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void builtin_eqv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOFEWARG,runnable,exe);
	fklNiReturn((fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
		fklNiEnd(&ap,stack);
}

void builtin_equal(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.equal",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.equal",FKL_ERR_TOOFEWARG,runnable,exe);
	fklNiReturn((fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void builtin_add(ARGL)
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.+",FKL_ERR_WRONGARG,runnable,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=0.0)
	{
		rd+=r64+fklBigIntToDouble(bi);
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
		fklFreeBigInt(bi);
	}
	else if(FKL_IS_0_BIG_INT(bi))
	{
		fklNiReturn(fklMakeVMint(r64,stack,exe->gc),&ap,stack);
		fklFreeBigInt(bi);
	}
	else
	{
		fklAddBigIntI(bi,r64);
		if(fklIsGtLtI64BigInt(bi))
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),stack,exe->gc),&ap,stack);
			fklFreeBigInt(bi);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_add_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.1+",runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&r,stack,exe->gc),&ap,stack);
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
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklBigIntToI64(arg->u.bigInt)+1,stack,exe->gc),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==INT64_MAX)
			{
				FklBigInt* bi=fklNewBigInt(i);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklGetInt(arg)+1,stack,exe->gc),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_sub(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"builtin.-",runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
		}
		else if(fklIsFixint(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64MulOverflow(p64,-1))
			{
				FklBigInt* bi=fklNewBigInt(p64);
				fklMulBigIntI(bi,-1);
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(-fklGetInt(prev),stack,exe->gc),&ap,stack);

		}
		else
		{
			FklBigInt* bi=fklNewBigInt0();
			fklSetBigInt(bi,prev->u.bigInt);
			fklMulBigIntI(bi,-1);
			if(fklIsGtLtI64BigInt(bi))
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			else
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),stack,exe->gc),&ap,stack);
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-",FKL_ERR_WRONGARG,runnable,exe);
			}
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=fklGetDouble(prev)-rd-r64-fklBigIntToDouble(bi);
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
			fklFreeBigInt(bi);
		}
		else if(FKL_IS_0_BIG_INT(bi)&&!FKL_IS_BIG_INT(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64AddOverflow(p64,-r64))
			{
				fklAddBigIntI(bi,p64);
				fklSubBigIntI(bi,r64);
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
			{
				r64=p64-r64;
				fklNiReturn(fklMakeVMint(r64,stack,exe->gc),&ap,stack);
				fklFreeBigInt(bi);
			}
		}
		else
		{
			fklSubBigInt(bi,prev->u.bigInt);
			fklMulBigIntI(bi,-1);
			fklSubBigIntI(bi,r64);
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_abs(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.abs",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.abs",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.abs",runnable,exe);
	if(FKL_IS_F64(obj))
	{
		double f=fabs(obj->u.f64);
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&f,stack,exe->gc),&ap,stack);
	}
	else
	{
		if(fklIsFixint(obj))
		{
			int64_t i=fklGetInt(obj);
			if(i==INT64_MIN)
			{
				FklBigInt* bi=fklNewBigInt(i);
				bi->neg=0;
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(i,stack,exe->gc),&ap,stack);
		}
		else
		{
			FklBigInt* bi=fklCopyBigInt(obj->u.bigInt);
			bi->neg=0;
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_sub_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.-1+",runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&r,stack,exe->gc),&ap,stack);
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
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklBigIntToI64(arg->u.bigInt)-1,stack,exe->gc),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==INT64_MIN)
			{
				FklBigInt* bi=fklNewBigInt(i);
				fklSubBigIntI(bi,1);
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklGetInt(arg)-1,stack,exe->gc),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_mul(ARGL)
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.*",FKL_ERR_WRONGARG,runnable,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=1.0)
	{
		rd*=r64*fklBigIntToDouble(bi);
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
		fklFreeBigInt(bi);
	}
	else if(FKL_IS_1_BIG_INT(bi))
	{
		fklNiReturn(fklMakeVMint(r64,stack,exe->gc),&ap,stack);
		fklFreeBigInt(bi);
	}
	else
	{
		fklMulBigIntI(bi,r64);
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_idiv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsInt,"builtin.//",runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(fklIsFixint(prev))
		{
			r64=fklGetInt(prev);
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,runnable,exe);
			if(r64==1)
				fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
			else
				fklNiReturn(FKL_MAKE_VM_I32(0),&ap,stack);
		}
		else
		{
			if(FKL_IS_0_BIG_INT(prev->u.bigInt))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,runnable,exe);
			if(FKL_IS_1_BIG_INT(prev->u.bigInt))
				fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
			else
				fklNiReturn(FKL_MAKE_VM_I32(0),&ap,stack);
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
			else
			{
				fklFreeBigInt(bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_WRONGARG,runnable,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi))
		{
			fklFreeBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,runnable,exe);
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_BIG_INT(prev)&&!fklIsGtLtI64BigInt(prev->u.bigInt)&&!FKL_IS_1_BIG_INT(bi))
		{
			FklBigInt* t=fklNewBigInt0();
			fklSetBigInt(t,prev->u.bigInt);
			fklDivBigInt(t,bi);
			fklDivBigIntI(t,r64);
			if(fklIsGtLtI64BigInt(t))
				fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,t,stack,exe->gc),&ap,stack);
			else
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(t),stack,exe->gc),&ap,stack);
				fklFreeBigInt(t);
			}
		}
		else
		{
			r64=fklGetInt(prev)/r64;
			fklNiReturn(fklMakeVMint(r64,stack,exe->gc),&ap,stack);
		}
		fklFreeBigInt(bi);
	}
	fklNiEnd(&ap,stack);
}

void builtin_div(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"builtin./",runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,runnable,exe);
			rd=1/prev->u.f64;
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
		}
		else
		{
			if(fklIsFixint(prev))
			{
				r64=fklGetInt(prev);
				if(!r64)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,runnable,exe);
				if(1%r64)
				{
					rd=1.0/r64;
					fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
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
					fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_WRONGARG,runnable,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi)||rd==0.0)
		{
			fklFreeBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,runnable,exe);
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
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&rd,stack,exe->gc),&ap,stack);
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
					fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,t,stack,exe->gc),&ap,stack);
				else
				{
					fklNiReturn(fklMakeVMint(fklBigIntToI64(t),stack,exe->gc),&ap,stack);
					fklFreeBigInt(t);
				}
			}
			else
			{
				r64=fklGetInt(prev)/r64;
				fklNiReturn(fklMakeVMint(r64,stack,exe->gc),&ap,stack);
			}
		}
		fklFreeBigInt(bi);
	}
	fklNiEnd(&ap,stack);
}

void builtin_mod(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_WRONGARG,runnable,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=fklGetDouble(fir);
		double as=fklGetDouble(sec);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,runnable,exe);
		double r=fmod(af,as);
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_F64,&r,stack,exe->gc),&ap,stack);
	}
	else if(fklIsFixint(fir)&&fklIsFixint(sec))
	{
		int64_t si=fklGetInt(sec);
		int64_t r=fklGetInt(fir)%si;
		if(si==0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,runnable,exe);
		fklNiReturn(fklMakeVMint(r,stack,exe->gc),&ap,stack);
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,runnable,exe);
			}
			fklModBigInt(rem,sec->u.bigInt);
		}
		else
		{
			int64_t si=fklGetInt(sec);
			if(si==0)
			{
				fklFreeBigInt(rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,runnable,exe);
			}
			fklModBigIntI(rem,si);
		}
		if(fklIsGtLtI64BigInt(rem))
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,rem,stack,exe->gc),&ap,stack);
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(rem),stack,exe->gc),&ap,stack);
			fklFreeBigInt(rem);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_eqn(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_TOOFEWARG,runnable,exe);
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
			else if(FKL_IS_BYTEVECTOR(prev)&&FKL_IS_BYTEVECTOR(prev))
				r=(fklBytevectorcmp(prev->u.bvec,cur->u.bvec)==0);
			else if(FKL_IS_CHR(prev)&&FKL_IS_CHR(cur))
				r=prev==cur;
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)==0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)==0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_WRONGARG,runnable,exe);
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

void builtin_gt(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_TOOFEWARG,runnable,exe);
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
			else if(FKL_IS_BYTEVECTOR(prev)&&FKL_IS_BYTEVECTOR(cur))
				r=(fklBytevectorcmp(prev->u.bvec,cur->u.bvec)>0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_WRONGARG,runnable,exe);
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

void builtin_ge(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_TOOFEWARG,runnable,exe);
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
			else if(FKL_IS_BYTEVECTOR(prev)&&FKL_IS_BYTEVECTOR(cur))
				r=(fklBytevectorcmp(prev->u.bvec,cur->u.bvec)>=0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_WRONGARG,runnable,exe);
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

void builtin_lt(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_TOOFEWARG,runnable,exe);
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
			else if(FKL_IS_BYTEVECTOR(prev)&&FKL_IS_BYTEVECTOR(cur))
				r=(fklBytevectorcmp(prev->u.bvec,cur->u.bvec)<0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_WRONGARG,runnable,exe);
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

void builtin_le(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_TOOFEWARG,runnable,exe);
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
			else if(FKL_IS_BYTEVECTOR(prev)&&FKL_IS_BYTEVECTOR(cur))
				r=(fklBytevectorcmp(prev->u.bvec,cur->u.bvec)<=0);
			else if(FKL_IS_USERDATA(prev)&&prev->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=prev->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_WRONGARG,runnable,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_WRONGARG,runnable,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_WRONGARG,runnable,exe);
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

void builtin_char_to_integer(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_CHR,"builtin.char->integer",runnable,exe);
	fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_integer_to_char(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->char",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->char",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsInt,"builtin.integer->char",runnable,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(fklGetInt(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->vector",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->vector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsList,"builtin.list->vector",runnable,exe);
	size_t len=fklVMlistLength(obj);
	FklVMvalue* r=fklNewVMvecV(len,NULL,stack,exe->gc);
	for(size_t i=0;obj!=FKL_VM_NIL;i++,obj=obj->u.pair->cdr)
		fklSetRef(&r->u.vec->base[i],obj->u.pair->car,exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->vector",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->vector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->vector",runnable,exe);
	size_t len=obj->u.str->size;
	FklVMvalue* r=fklNewVMvecV(len,NULL,stack,exe->gc);
	for(size_t i=0;i<len;i++)
		fklSetRef(&r->u.vec->base[i],FKL_MAKE_VM_CHR(obj->u.str->str[i]),exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-vector",runnable,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOMANYARG,runnable,exe);
	size_t len=fklGetInt(size);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	FklVMvalue* t=content?content:FKL_VM_NIL;
	for(size_t i=0;i<len;i++)
	{
		fklSetRef(cur,fklNewVMvalue(FKL_TYPE_PAIR,fklNewVMpair(),gc),gc);
		fklSetRef(&(*cur)->u.pair->car,t,gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->list",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->list",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->list",runnable,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklString* str=obj->u.str;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<str->size;i++)
	{
		fklSetRef(cur,fklNewVMvalue(FKL_TYPE_PAIR,fklNewVMpair(),gc),gc);
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_CHR(str->str[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_s8_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-list",runnable,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	int8_t* s8a=(int8_t*)obj->u.bvec->ptr;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklNewVMvalue(FKL_TYPE_PAIR,fklNewVMpair(),gc),gc);
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_I32(s8a[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_u8_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-list",runnable,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	uint8_t* u8a=obj->u.bvec->ptr;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklNewVMvalue(FKL_TYPE_PAIR,fklNewVMpair(),gc),gc);
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_I32(u8a[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_s8_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-vector",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-vector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-vector",runnable,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	int8_t* s8a=(int8_t*)obj->u.bvec->ptr;
	FklVMvalue* vec=fklNewVMvecV(obj->u.bvec->size,NULL,stack,exe->gc);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_I32(s8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_u8_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->u8-vector",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->u8-vector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->u8-vector",runnable,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	uint8_t* u8a=obj->u.bvec->ptr;
	FklVMvalue* vec=fklNewVMvecV(obj->u.bvec->size,NULL,stack,exe->gc);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_I32(u8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->list",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->list",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_VECTOR,"builtin.vector->list",runnable,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvec* vec=obj->u.vec;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<vec->size;i++)
	{
		fklSetRef(cur,fklNewVMvalue(FKL_TYPE_PAIR,fklNewVMpair(),gc),gc);
		fklSetRef(&(*cur)->u.pair->car,vec->base[i],gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	size_t size=ap-stack->bp;
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,NULL),stack,gc);
	FklString* str=r->u.str;
	size_t i=0;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur!=NULL
			;cur=fklNiGetArg(&ap,stack),i++)
	{
		FKL_NI_CHECK_TYPE(cur,FKL_IS_CHR,"builtin.string",runnable,exe);
		str->str[i]=FKL_GET_CHR(cur);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-string",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-string",runnable,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-string",FKL_ERR_TOOMANYARG,runnable,exe);
	size_t len=fklGetInt(size);
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(len,NULL),stack,gc);
	FklString* str=r->u.str;
	char ch=0;
	if(content)
	{
		FKL_NI_CHECK_TYPE(content,FKL_IS_CHR,"builtin.make-string",runnable,exe);
		ch=FKL_GET_CHR(content);
	}
	memset(str->str,ch,len);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-vector",runnable,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOMANYARG,runnable,exe);
	size_t len=fklGetInt(size);
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_VECTOR,fklNewVMvec(len),stack,gc);
	if(content)
	{
		FklVMvec* vec=r->u.vec;
		for(size_t i=0;i<len;i++)
			fklSetRef(&vec->base[i],content,gc);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_STR,"builtin.sub-string",runnable,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-string",runnable,exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.sub-string",runnable,exe);
	size_t size=ostr->u.str->size;
	ssize_t start=fklGetInt(vstart);
	ssize_t end=fklGetInt(vend);
	if(start<0||start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_INVALIDACCESS,runnable,exe);
	size=end-start;
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,ostr->u.str->str+start),stack,gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,"builtin.sub-bytevector",runnable,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-bytevector",runnable,exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.sub-bytevector",runnable,exe);
	size_t size=ostr->u.str->size;
	ssize_t start=fklGetInt(vstart);
	ssize_t end=fklGetInt(vend);
	if(start<0||start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_INVALIDACCESS,runnable,exe);
	size=end-start;
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(size,ostr->u.bvec->ptr+start),stack,gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* ovec=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ovec||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ovec,FKL_IS_VECTOR,"builtin.sub-vector",runnable,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-vector",runnable,exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.sub-vector",runnable,exe);
	size_t size=ovec->u.vec->size;
	ssize_t start=fklGetInt(vstart);
	ssize_t end=fklGetInt(vend);
	if(start<0||start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_INVALIDACCESS,runnable,exe);
	size=end-start;
	FklVMvalue* r=fklNewVMvecV(size,ovec->u.vec->base+start,stack,gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(FKL_IS_SYM(obj))
		retval=fklNewVMvalueToStack(FKL_TYPE_STR
				,fklCopyString(fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol)
				,stack,exe->gc);
	else if(FKL_IS_CHR(obj))
	{
		FklString* r=fklNewString(1,NULL);
		r->str[0]=FKL_GET_CHR(obj);
		retval=fklNewVMvalueToStack(FKL_TYPE_STR
				,r
				,stack,exe->gc);
	}
	else if(fklIsVMnumber(obj))
	{
		retval=fklNewVMvalueToStack(FKL_TYPE_STR,NULL,stack,exe->gc);
		if(fklIsInt(obj))
		{
			if(FKL_IS_BIG_INT(obj))
				retval->u.str=fklBigIntToString(obj->u.bigInt,10);
			else
			{
				FklBigInt bi=FKL_BIG_INT_INIT;
				uint8_t t[64]={0};
				bi.size=64;
				bi.digits=t;
				fklSetBigIntI(&bi,fklGetInt(obj));
				retval->u.str=fklBigIntToString(&bi,10);
			}
		}
		else
		{
			char buf[64]={0};
			size_t size=snprintf(buf,64,"%lf",obj->u.f64);
			retval->u.str=fklNewString(size,buf);
		}
	}
	else if(FKL_IS_BYTEVECTOR(obj))
		retval=fklNewVMvalueToStack(FKL_TYPE_STR
				,fklNewString(obj->u.bvec->size,(char*)obj->u.bvec->ptr)
				,stack,exe->gc);
	else if(FKL_IS_VECTOR(obj))
	{
		size_t size=obj->u.vec->size;
		retval=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,NULL),stack,exe->gc);
		for(size_t i=0;i<size;i++)
		{
			FKL_NI_CHECK_TYPE(obj->u.vec->base[i],FKL_IS_CHR,"builtin.->string",runnable,exe);
			retval->u.str->str[i]=FKL_GET_CHR(obj->u.vec->base[i]);
		}
	}
	else if(fklIsList(obj))
	{
		size_t size=fklVMlistLength(obj);
		retval=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,NULL),stack,exe->gc);
		for(size_t i=0;i<size;i++)
		{
			FKL_NI_CHECK_TYPE(obj->u.pair->car,FKL_IS_CHR,"builtin.->string",runnable,exe);
			retval->u.str->str[i]=FKL_GET_CHR(obj->u.pair->car);
			obj=obj->u.pair->cdr;
		}
	}
	else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__to_string)
		retval=fklNewVMvalueToStack(FKL_TYPE_STR,obj->u.ud->t->__to_string(obj->u.ud->data),stack,exe->gc);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_WRONGARG,runnable,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_symbol_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->string",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->string",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_SYM,"builtin.symbol->string",runnable,exe);
	FklVMvalue* retval=fklNewVMvalueToStack(FKL_TYPE_STR
			,fklCopyString(fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol)
			,stack,exe->gc);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_symbol(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->symbol",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->symbol",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->symbol",runnable,exe);
	fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbolToGlob(obj->u.str)->id),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_symbol_to_integer(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->integer",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->integer",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_SYM,"builtin.symbol->integer",runnable,exe);
	FklVMvalue* r=fklMakeVMint(FKL_GET_SYM(obj),stack,exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_number(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->number",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->number",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->number",runnable,exe);
	FklVMvalue* r=FKL_VM_NIL;
	if(fklIsNumberString(obj->u.str))
	{
		if(fklIsDoubleString(obj->u.str))
		{
			double d=fklStringToDouble(obj->u.str);
			r=fklNewVMvalueToStack(FKL_TYPE_F64,&d,stack,exe->gc);
		}
		else
		{
			FklBigInt* bi=fklNewBigIntFromString(obj->u.str);
			if(!fklIsGtLtI64BigInt(bi))
			{
				r=fklMakeVMint(fklBigIntToI64(bi),stack,exe->gc);
				fklFreeBigInt(bi);
			}
			else
				r=fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc);
		}
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* radix=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->string",runnable,exe);
	FklVMvalue* retval=fklNewVMvalueToStack(FKL_TYPE_STR,NULL,stack,exe->gc);
	if(fklIsInt(obj))
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_NI_CHECK_TYPE(radix,fklIsInt,"builtin.number->string",runnable,exe);
			int64_t t=fklGetInt(radix);
			if(t!=8&&t!=10&&t!=16)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_INVALIDRADIX,runnable,exe);
			base=t;
		}
		if(FKL_IS_BIG_INT(obj))
			retval->u.str=fklBigIntToString(obj->u.bigInt,base);
		else
		{
			FklBigInt bi=FKL_BIG_INT_INIT;
			uint8_t t[64]={0};
			bi.size=64;
			bi.digits=t;
			fklSetBigIntI(&bi,fklGetInt(obj));
			retval->u.str=fklBigIntToString(&bi,base);
		}
	}
	else
	{
		char buf[256]={0};
		if(radix)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOMANYARG,runnable,exe);
		size_t size=snprintf(buf,64,"%lf",obj->u.f64);
		retval->u.str=fklNewString(size,buf);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->string",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.vector->string",runnable,exe);
	size_t size=vec->u.vec->size;
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,NULL),stack,exe->gc);
	for(size_t i=0;i<size;i++)
	{
		FKL_NI_CHECK_TYPE(vec->u.vec->base[i],FKL_IS_CHR,"builtin.vector->string",runnable,exe);
		r->u.str->str[i]=FKL_GET_CHR(vec->u.vec->base[i]);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->string",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_BYTEVECTOR,"builtin.bytevector->string",runnable,exe);
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(vec->u.bvec->size,(char*)vec->u.bvec->ptr),stack,exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->string",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.list->string",runnable,exe);
	size_t size=fklVMlistLength(list);
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,NULL),stack,exe->gc);
	for(size_t i=0;i<size;i++)
	{
		FKL_NI_CHECK_TYPE(list->u.pair->car,FKL_IS_CHR,"builtin.list->string",runnable,exe);
		r->u.str->str[i]=FKL_GET_CHR(list->u.pair->car);
		list=list->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_f64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalueToStack(FKL_TYPE_F64,NULL,stack,exe->gc);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.integer->f64",runnable,exe);
	if(fklIsFixint(obj))
		retval->u.f64=(double)fklGetInt(obj);
	else if(FKL_IS_BIG_INT(obj))
		retval->u.f64=fklBigIntToDouble(obj->u.bigInt);
	else
		retval->u.f64=obj->u.f64;
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_integer(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->integer",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->integer",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->integer",runnable,exe);
	if(FKL_IS_F64(obj))
	{
		if(obj->u.f64-(double)INT64_MAX>DBL_EPSILON||obj->u.f64-INT64_MIN<-DBL_EPSILON)
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,fklNewBigIntD(obj->u.f64),stack,exe->gc),&ap,stack);
		else
			fklNiReturn(fklMakeVMintD(obj->u.f64,stack,exe->gc),&ap,stack);
	}
	else if(FKL_IS_BIG_INT(obj))
	{
		if(fklIsGtLtI64BigInt(obj->u.bigInt))
		{
			FklBigInt* bi=fklNewBigInt0();
			fklSetBigInt(bi,obj->u.bigInt);
			fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
		}
		else
			fklNiReturn(fklMakeVMint(fklBigIntToI64(obj->u.bigInt),stack,exe->gc),&ap,stack);
	}
	else
		fklNiReturn(fklMakeVMint(fklGetInt(obj),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_i32(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i32",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i32",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->i32",runnable,exe);
	int32_t r=0;
	if(FKL_IS_F64(obj))
		r=obj->u.f64;
	else
		r=fklGetInt(obj);
	fklNiReturn(FKL_MAKE_VM_I32(r),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_big_int(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->big-int",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->big-int",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->big-int",runnable,exe);
	FklBigInt* bi=NULL;
	if(FKL_IS_F64(obj))
		bi=fklNewBigIntD(obj->u.f64);
	else if(FKL_IS_BIG_INT(obj))
		bi=fklCopyBigInt(obj->u.bigInt);
	else
		bi=fklNewBigInt(fklGetInt(obj));
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bi,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_i64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i64",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i64",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->i64",runnable,exe);
	int64_t r=0;
	if(FKL_IS_F64(obj))
		r=obj->u.f64;
	else
		r=fklGetInt(obj);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_I64,&r,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nth",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INVALIDACCESS,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void builtin_set_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.set-nth!",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			fklSetRef(&objPair->u.pair->car,target,exe->gc);
			fklNiReturn(target,&ap,stack);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INVALIDASSIGN,runnable,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void builtin_sref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!str)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=str->u.str->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,runnable,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(str->u.str->str[index]),&ap,stack);
	fklNiEnd(&ap,stack);
}

#define BV_U_S_8_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_WRONGARG,runnable,exe);\
	int64_t index=fklGetInt(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(index<0||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,runnable,exe);\
	r=bvec->u.bvec->ptr[index];\
	fklNiReturn(fklMakeVMint(r,stack,exe->gc),&ap,stack);\
	fklNiEnd(&ap,stack);

#define BV_LT_U64_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_WRONGARG,runnable,exe);\
	int64_t index=fklGetInt(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(index<0||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,runnable,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
		((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];\
	fklNiReturn(fklMakeVMint(r,stack,exe->gc),&ap,stack);\
	fklNiEnd(&ap,stack);

void builtin_bvs8ref(ARGL) {BV_U_S_8_REF(int8_t,"builtin.bvs8ref")}
void builtin_bvs16ref(ARGL) {BV_LT_U64_REF(int16_t,"builtin.bvs16ref")}
void builtin_bvs32ref(ARGL) {BV_LT_U64_REF(int32_t,"builtin.bvs32ref")}
void builtin_bvs64ref(ARGL) {BV_LT_U64_REF(int64_t,"builtin.bvs64ref")}

void builtin_bvu8ref(ARGL) {BV_U_S_8_REF(uint8_t,"builtin.bvu8ref")}
void builtin_bvu16ref(ARGL) {BV_LT_U64_REF(uint16_t,"builtin.bvu16ref")}
void builtin_bvu32ref(ARGL) {BV_LT_U64_REF(uint32_t,"builtin.bvu32ref")}
void builtin_bvu64ref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!bvec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=bvec->u.bvec->size;
	uint64_t r=0;
	if(index<0||index>=size||size-index<sizeof(r))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_INVALIDACCESS,runnable,exe);
	for(size_t i=0;i<sizeof(r);i++)
		((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];
	if(r>=INT64_MAX)
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BIG_INT,fklNewBigIntU(r),stack,exe->gc),&ap,stack);
	else
		fklNiReturn(fklMakeVMint(r,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}
#undef BV_LT_U64_REF
#undef BV_U_S_8_REF

#define BV_F_REF(TYPE,WHO) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_WRONGARG,runnable,exe);\
	int64_t index=fklGetInt(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(index<0||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,runnable,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];\
	FklVMvalue* f=fklNewVMvalueToStack(FKL_TYPE_F64,NULL,stack,exe->gc);\
	f->u.f64=r;\
	fklNiReturn(f,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void builtin_bvf32ref(ARGL) BV_F_REF(float,"builtin.bvf32ref")
void builtin_bvf64ref(ARGL) BV_F_REF(double,"builtin.bvf32ref")
#undef BV_F_REF

#define SET_BV_LE_U8_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_WRONGARG,runnable,exe);\
	int64_t index=fklGetInt(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=fklGetInt(target);\
	if(index<0||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,runnable,exe);\
	bvec->u.bvec->ptr[index]=r;\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);

#define SET_BV_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_WRONGARG,runnable,exe);\
	int64_t index=fklGetInt(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=fklGetInt(target);\
	if(index<0||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,runnable,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	bvec->u.bvec->ptr[index+i]=((uint8_t*)&r)[i];\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);

void builtin_set_bvs8ref(ARGL) {SET_BV_LE_U8_REF(int8_t,"builtin.set-bvs8ref!")}
void builtin_set_bvs16ref(ARGL) {SET_BV_REF(int16_t,"builtin.set-bvs16ref!")}
void builtin_set_bvs32ref(ARGL) {SET_BV_REF(int32_t,"builtin.set-bvs32ref!")}
void builtin_set_bvs64ref(ARGL) {SET_BV_REF(int64_t,"builtin.set-bvs64ref!")}

void builtin_set_bvu8ref(ARGL) {SET_BV_LE_U8_REF(uint8_t,"builtin.set-bvu8ref!")}
void builtin_set_bvu16ref(ARGL) {SET_BV_REF(uint16_t,"builtin.set-bvu16ref!")}
void builtin_set_bvu32ref(ARGL) {SET_BV_REF(uint32_t,"builtin.set-bvu32ref!")}
void builtin_set_bvu64ref(ARGL) {SET_BV_REF(uint64_t,"builtin.set-bvu64ref!")}
#undef SET_BV_IU_REF

#define SET_BV_F_REF(TYPE,WHO) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!FKL_IS_F64(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_WRONGARG,runnable,exe);\
	int64_t index=fklGetInt(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=target->u.f64;\
	if(index<0||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,runnable,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	bvec->u.bvec->ptr[index+i]=((uint8_t*)&r)[i];\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void builtin_set_bvf32ref(ARGL) SET_BV_F_REF(float,"builtin.set-bvf32ref!")
void builtin_set_bvf64ref(ARGL) SET_BV_F_REF(double,"builtin.set-bvf64ref!")
#undef SET_BV_F_REF

void builtin_set_sref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!str||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=str->u.str->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(!FKL_IS_CHR(target)&&!fklIsInt(target))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_WRONGARG,runnable,exe);
	str->u.str->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!str||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHR(content)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_WRONGARG,runnable,exe);
	memset(str->u.str->str,FKL_GET_CHR(content),str->u.str->size);
	fklNiReturn(str,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!bvec||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(content)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_WRONGARG,runnable,exe);
	memset(bvec->u.bvec->ptr,fklGetInt(content),bvec->u.bvec->size);
	fklNiReturn(bvec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!vector)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,runnable,exe);
	fklNiReturn(vector->u.vec->base[index],&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!vector||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INVALIDACCESS,runnable,exe);
	fklSetRef(&vector->u.vec->base[index],target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-vector!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!vec||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-vector!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.fill-vector!",runnable,exe);
	for(size_t i=0;i<vec->u.vec->size;i++)
		fklSetRef(&vec->u.vec->base[i],content,exe->gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cas_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* new_=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!vector||!old||!new_)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_WRONGARG,runnable,exe);
	int64_t index=fklGetInt(place);
	size_t size=vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(vector->u.vec->base[index]==old)
	{
		fklSetRef(&vector->u.vec->base[index],new_,exe->gc);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_nthcdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nthcdr",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_INVALIDACCESS,runnable,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void builtin_set_nthcdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.set-nthcdr!",runnable,exe);
	int64_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			fklSetRef(&objPair->u.pair->cdr,target,exe->gc);
			fklNiReturn(target,&ap,stack);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INVALIDACCESS,runnable,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void builtin_length(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_TOOFEWARG,runnable,exe);
	size_t len=0;
	if(obj==FKL_VM_NIL||FKL_IS_PAIR(obj))
		len=fklVMlistLength(obj);
	else if(FKL_IS_STR(obj))
		len=obj->u.str->size;
	else if(FKL_IS_VECTOR(obj))
		len=obj->u.vec->size;
	else if(FKL_IS_BYTEVECTOR(obj))
		len=obj->u.bvec->size;
	else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__length)
		len=obj->u.ud->t->__length(obj->u.ud->data);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_WRONGARG,runnable,exe);
	fklNiReturn(fklMakeVMint(len,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fopen(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMgc* gc=exe->gc;
	FklVMvalue* filename=fklNiGetArg(&ap,stack);
	FklVMvalue* mode=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_WRONGARG,runnable,exe);
	char c_filename[filename->u.str->size+1];
	char c_mode[mode->u.str->size+1];
	fklWriteStringToCstr(c_filename,filename->u.str);
	fklWriteStringToCstr(c_mode,mode->u.str);
	FILE* file=fopen(c_filename,c_mode);
	FklVMvalue* obj=NULL;
	if(!file)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.fopen",c_filename,1,FKL_ERR_FILEFAILURE,exe);
	else
		obj=fklNewVMvalueToStack(FKL_TYPE_FP,fklNewVMfp(file),stack,gc);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fclose(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.fclose",runnable,exe);
	if(fp->u.fp==NULL||fklFreeVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_INVALIDACCESS,runnable,exe);
	fp->u.fp=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_read(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream)&&!FKL_IS_STR(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_WRONGARG,runnable,exe);
	char* tmpString=NULL;
	FklVMfp* tmpFile=NULL;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	if(!stream||FKL_IS_FP(stream))
	{
		tmpFile=stream?stream->u.fp:BuiltInStdin->u.fp;
		int unexpectEOF=0;
		size_t size=0;
		tmpString=fklReadInStringPattern(tmpFile->fp,(char**)&tmpFile->prev,&size,&tmpFile->size,0,&unexpectEOF,tokenStack,NULL);
		if(unexpectEOF)
		{
			free(tmpString);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_UNEXPECTEOF,runnable,exe);
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
		tmp=fklCastCptrVMvalue(tmpCptr,exe->gc);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklFreeToken(fklPopPtrStack(tokenStack));
	fklFreePtrStack(tokenStack);
	fklNiReturn(tmp,&ap,stack);
	free(tmpString);
	fklDeleteCptr(tmpCptr);
	free(tmpCptr);
	fklNiEnd(&ap,stack);
}

void builtin_fgets(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_WRONGARG,runnable,exe);
	FklVMfp* fp=file->u.fp;
	size_t size=fklGetInt(psize);
	char* str=(char*)malloc(sizeof(char)*size);
	FKL_ASSERT(str);
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
		FklVMvalue* vmstr=fklNewVMvalueToStack(FKL_TYPE_STR,NULL,stack,exe->gc);
		vmstr->u.str=(FklString*)malloc(sizeof(FklString)+fklGetInt(psize));
		FKL_ASSERT(vmstr->u.str);
		vmstr->u.str->size=fklGetInt(psize);
		memcpy(vmstr->u.str->str,str,fklGetInt(psize));
		free(str);
		fklNiReturn(vmstr,&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_fgetb(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_WRONGARG,runnable,exe);
	FklVMfp* fp=file->u.fp;
	size_t size=fklGetInt(psize);
	uint8_t* ptr=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(ptr);
	int32_t realRead=0;
	if(fp->size)
	{
		if(fp->size<=size)
		{
			memcpy(ptr,fp->prev,fp->size);
			realRead+=fp->size;
			size-=fp->size;
			free(fp->prev);
			fp->prev=NULL;
			fp->size=0;
		}
		else
		{
			fp->size-=size;
			memcpy(ptr,fp->prev,size);
			realRead+=size;
			uint8_t* prev=fp->prev;
			fp->prev=fklCopyMemory(prev+size,sizeof(uint8_t)*fp->size);
			free(prev);
			size=0;
		}
	}
	if(size)
		realRead+=fread(ptr,sizeof(uint8_t),size,fp->fp);
	if(!realRead)
	{
		free(ptr);
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
	{
		FklVMvalue* bvec=fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(realRead,ptr),stack,exe->gc);
		free(ptr);
		fklNiReturn(bvec,&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_prin1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrin1VMvalue(obj,objFile);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_princ(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrincVMvalue(obj,objFile);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_dlopen(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* dllName=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlopen",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlopen",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(dllName,FKL_IS_STR,"builtin.dlopen",runnable,exe);
	char str[dllName->u.str->size+1];
	fklWriteStringToCstr(str,dllName->u.str);
	FklVMdllHandle* dll=fklNewVMdll(str);
	if(!dll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.dlopen",str,1,FKL_ERR_LOADDLLFAILD,exe);
	FklVMvalue* rel=fklNewVMvalueToStack(FKL_TYPE_DLL,dll,stack,exe->gc);
	fklInitVMdll(rel);
	fklNiReturn(rel,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_dlsym(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMgc* gc=exe->gc;
	FklVMvalue* dll=fklNiGetArg(&ap,stack);
	FklVMvalue* symbol=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(dll))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_WRONGARG,runnable,exe);
	if(!dll->u.dll)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_INVALIDACCESS,runnable,exe);
	char str[symbol->u.str->size+1];
	fklWriteStringToCstr(str,symbol->u.str);
	FklVMdllFunc funcAddress=fklGetAddress(str,dll->u.dll);
	if(!funcAddress)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.dlsym",str,1,FKL_ERR_INVALIDSYMBOL,exe);
	FklVMdlproc* dlproc=fklNewVMdlproc(funcAddress,dll);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_DLPROC,dlproc,stack,gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_argv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* retval=NULL;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.argv",FKL_ERR_TOOMANYARG,exe->rhead,exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&(*tmp)->u.pair->cdr)
		*tmp=fklNewVMpairV(fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(strlen(argv[i]),argv[i]),stack,exe->gc),FKL_VM_NIL,stack,exe->gc);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_go(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(!exe->thrds)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_CANTCREATETHREAD,runnable,exe);
	FklVMvalue* threadProc=fklNiGetArg(&ap,stack);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!FKL_IS_CONT(threadProc)&&!fklIsCallableUd(threadProc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_WRONGARG,runnable,exe);
	FklVM* threadVM=FKL_IS_PROC(threadProc)?fklNewThreadVM(threadProc->u.proc,exe->gc):fklNewThreadCallableObjVM(runnable,exe->gc,threadProc);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	FklVMstack* threadVMstack=threadVM->stack;
	fklNiSetBp(threadVMstack->tp,threadVMstack);
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
	faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMfunc,threadVM);
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

void builtin_chanl(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* maxSize=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(maxSize,fklIsInt,"builtin.chanl",runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_CHAN,fklNewVMchanl(fklGetInt(maxSize)),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

//void builtin_chanl_mes_num(ARGL)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMrunnable* runnable=exe->rhead;
//	FklVMvalue* obj=fklNiGetArg(&ap,stack);
//	if(fklNiResBp(&ap,stack))
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-mes-num",FKL_ERR_TOOMANYARG,runnable,exe);
//	if(!obj)
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-mes-num",FKL_ERR_TOOFEWARG,runnable,exe);
//	size_t len=0;
//	if(FKL_IS_CHAN(obj))
//		len=obj->u.chan->messageNum;
//	else
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-mes-num",FKL_ERR_WRONGARG,runnable,exe);
//	fklNiReturn(fklMakeVMint(len,stack,exe->gc),&ap,stack);
//	fklNiEnd(&ap,stack);
//}

void builtin_send(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.send",runnable,exe);
	FklVMsend* t=fklNewVMsend(message);
	fklChanlSend(t,ch->u.chan);
	fklNiReturn(message,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_recv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	FklVMvalue* okBox=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.recv",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.recv",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.recv",runnable,exe);
	if(okBox)
	{
		FKL_NI_CHECK_TYPE(okBox,FKL_IS_BOX,"builtin.recv",runnable,exe);
		FklVMvalue* r=FKL_VM_NIL;
		int ok=0;
		fklChanlRecvOk(ch->u.chan,&r,&ok);
		if(ok)
			okBox->u.box=FKL_VM_TRUE;
		else
			okBox->u.box=FKL_VM_NIL;
		fklNiReturn(r,&ap,stack);
	}
	else
	{
		FklVMrecv* t=fklNewVMrecv();
		fklChanlRecv(t,ch->u.chan);
		fklNiReturn(t->v,&ap,stack);
		fklFreeVMrecv(t);
	}
	fklNiEnd(&ap,stack);
}

void builtin_error(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* who=fklNiGetArg(&ap,stack);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_WRONGARG,runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_ERR,fklNewVMerror((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:who->u.str,FKL_GET_SYM(type),message->u.str),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_raise(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* err=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.raise",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.raise",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(err,FKL_IS_ERR,"builtin.raise",runnable,exe);
	fklNiEnd(&ap,stack);
	fklRaiseVMerror(err,exe);
}

void builtin_call_cc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/cc",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/cc",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.call/cc",runnable,exe);
	pthread_rwlock_rdlock(&exe->rlock);
	FklVMcontinuation* cc=fklNewVMcontinuation(ap,exe);
	if(!cc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/cc",FKL_ERR_CROSS_C_CALL_CONTINUATION,runnable,exe);
	FklVMvalue* vcc=fklNewVMvalueToStack(FKL_TYPE_CONT,cc,stack,exe->gc);
	pthread_rwlock_unlock(&exe->rlock);
	fklNiSetBp(ap,stack);
	fklNiReturn(vcc,&ap,stack);
	if(proc->type==FKL_TYPE_PROC)
	{
		FklVMproc* tmpProc=proc->u.proc;
		FklVMrunnable* prevProc=fklHasSameProc(tmpProc->scp,exe->rhead);
		if(fklIsTheLastExpress(runnable,prevProc,exe)&&prevProc)
			prevProc->mark=1;
		else
		{
			FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc,exe->rhead);
			tmpRunnable->localenv=fklNewVMvalueToStack(FKL_TYPE_ENV,fklNewVMenv(tmpProc->prevEnv,exe->gc),stack,exe->gc);
			exe->rhead=tmpRunnable;
		}
	}
	else
		exe->nextCall=proc;
	fklNiEnd(&ap,stack);
}

void builtin_apply(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.apply",runnable,exe);
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
	if(!lastList)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_TOOFEWARG,runnable,exe);
	FklPtrStack* stack2=fklNewPtrStack(32,16);
	if(!FKL_IS_PAIR(lastList)&&lastList!=FKL_VM_NIL)
	{
		fklFreePtrStack(stack1);
		fklFreePtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_WRONGARG,runnable,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=fklGetVMpairCdr(lastList))
		fklPushPtrStack(fklGetVMpairCar(lastList),stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklFreePtrStack(stack1);
		fklFreePtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_WRONGARG,runnable,exe);
	}
	fklNiSetBp(ap,stack);
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
		case FKL_TYPE_PROC:
			applyNativeProc(exe,proc->u.proc,runnable);
			break;
		default:
			exe->nextCall=proc;
			break;
	}
	fklNiEnd(&ap,stack);
}

typedef struct
{
	FklVMvalue** cur;
	FklVMvalue** r;
	FklVMvalue* proc;
	FklVMvalue* vec;
	FklVMvalue* cars;
	size_t i;
	size_t len;
	size_t num;
	uint32_t ap;
}MapCtx;

#define K_MAP_PATTERN(K_FUNC,CUR_PROCESS,RESULT_PROCESS,NEXT_PROCESS) {MapCtx* mapctx=(MapCtx*)ctx;\
	FklVMgc* gc=exe->gc;\
	size_t len=mapctx->len;\
	size_t argNum=mapctx->num;\
	FklVMstack* stack=exe->stack;\
	FklVMrunnable* runnable=exe->rhead;\
	if(s==FKL_CC_OK)\
	fklNiSetTp(exe->stack);\
	else if(s==FKL_CC_RE)\
	{\
		FklVMvalue* result=fklGetTopValue(exe->stack);\
		RESULT_PROCESS\
		NEXT_PROCESS\
		fklNiResTp(exe->stack);\
		mapctx->i++;\
	}\
	while(mapctx->i<len)\
	{\
		CUR_PROCESS\
		for(size_t i=0;i<argNum;i++)\
		{\
			FklVMvalue* pair=mapctx->vec->u.vec->base[i];\
			fklSetRef(&(mapctx->cars)->u.vec->base[i],pair->u.pair->car,gc);\
			fklSetRef(&mapctx->vec->u.vec->base[i],pair->u.pair->cdr,gc);\
		}\
		fklVMcallInDlproc(mapctx->proc,argNum,mapctx->cars->u.vec->base,runnable,exe,(K_FUNC),mapctx,sizeof(MapCtx));\
		FklVMvalue* result=fklGetTopValue(exe->stack);\
		RESULT_PROCESS\
		NEXT_PROCESS\
		fklNiResTp(exe->stack);\
		mapctx->i++;\
	}\
	fklNiPopTp(stack);\
	fklNiReturn(*mapctx->r,&mapctx->ap,stack);\
	fklNiEnd(&mapctx->ap,stack);\
	free(ctx);}

#define MAP_PATTERN(FUNC_NAME,K_FUNC,DEFAULT_VALUE) FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* proc=fklNiGetArg(&ap,stack);\
	FklVMgc* gc=exe->gc;\
	if(!proc)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,runnable,exe);\
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,(FUNC_NAME),runnable,exe);\
	size_t argNum=ap-stack->bp;\
	if(argNum==0)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,runnable,exe);\
	FklVMvalue* argVec=fklNewVMvecV(ap-stack->bp,NULL,stack,gc);\
	for(size_t i=0;i<argNum;i++)\
	{\
		FklVMvalue* cur=fklNiGetArg(&ap,stack);\
		if(!fklIsList(cur))\
		FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_WRONGARG,runnable,exe);\
		fklSetRef(&argVec->u.vec->base[i],cur,gc);\
	}\
	fklNiResBp(&ap,stack);\
	size_t len=fklVMlistLength(argVec->u.vec->base[0]);\
	for(size_t i=1;i<argNum;i++)\
	if(fklVMlistLength(argVec->u.vec->base[i])!=len)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_LIST_DIFFER_IN_LENGTH,runnable,exe);\
	if(len==0)\
	{\
		fklNiReturn((DEFAULT_VALUE),&ap,stack);\
		fklNiEnd(&ap,stack);\
	}\
	else\
	{\
		FklVMvalue* cars=fklNewVMvecV(argNum,NULL,stack,gc);\
		MapCtx* mapctx=(MapCtx*)malloc(sizeof(MapCtx));\
		FKL_ASSERT(mapctx);\
		fklPushVMvalue((DEFAULT_VALUE),stack);\
		mapctx->proc=proc;\
		mapctx->r=fklNiGetTopSlot(stack);\
		mapctx->ap=ap;\
		mapctx->cars=cars;\
		mapctx->cur=mapctx->r;\
		mapctx->i=0;\
		mapctx->len=len;\
		mapctx->num=argNum;\
		mapctx->vec=argVec;\
		(K_FUNC)(exe,FKL_CC_OK,mapctx);}\

static void k_map(K_FUNC_ARGL) {K_MAP_PATTERN(k_map,
		*(mapctx->cur)=fklNewVMvalueToStack(FKL_TYPE_PAIR,fklNewVMpair(),stack,gc);,
		fklSetRef(&(*mapctx->cur)->u.pair->car,result,gc);,
		mapctx->cur=&(*mapctx->cur)->u.pair->cdr;)}
void builtin_map(ARGL) {MAP_PATTERN("builtin.map",k_map,FKL_VM_NIL)}

static void k_foreach(K_FUNC_ARGL) {K_MAP_PATTERN(k_foreach,,*(mapctx->r)=result;,)}
void builtin_foreach(ARGL) {MAP_PATTERN("builtin.foreach",k_foreach,FKL_VM_NIL)}

static void k_andmap(K_FUNC_ARGL) {K_MAP_PATTERN(k_andmap,,*(mapctx->r)=result;if(result==FKL_VM_NIL)mapctx->i=len;,)}
void builtin_andmap(ARGL) {MAP_PATTERN("builtin.andmap",k_andmap,FKL_VM_TRUE)}

static void k_ormap(K_FUNC_ARGL) {K_MAP_PATTERN(k_ormap,,*(mapctx->r)=result;if(result!=FKL_VM_NIL)mapctx->i=len;,)}
void builtin_ormap(ARGL) {MAP_PATTERN("builtin.ormap",k_ormap,FKL_VM_NIL)}

#undef K_MAP_PATTERN
#undef MAP_PATTERN

void builtin_memq(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!obj||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memq",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memq",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.memq",runnable,exe);
	FklVMvalue* r=list;
	for(;r!=FKL_VM_NIL;r=r->u.pair->cdr)
		if(r->u.pair->car==obj)
			break;
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

typedef struct
{
	FklVMvalue** r;
	FklVMvalue* obj;
	FklVMvalue* proc;
	FklVMvalue* list;
	uint32_t ap;
}MemberCtx;

static void k_member(K_FUNC_ARGL)
{
	MemberCtx* memberctx=(MemberCtx*)ctx;
	FklVMstack* stack=exe->stack;
	if(s==FKL_CC_OK)
		fklNiSetTp(stack);
	else if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*memberctx->r=memberctx->list;
			memberctx->list=FKL_VM_NIL;
		}
		else
			memberctx->list=memberctx->list->u.pair->cdr;
		fklNiResTp(stack);
	}
	while(memberctx->list!=FKL_VM_NIL)
	{
		FklVMvalue* arglist[2]={memberctx->obj,memberctx->list->u.pair->car};
		fklVMcallInDlproc(memberctx->proc
				,2,arglist
				,exe->rhead,exe,k_member,memberctx,sizeof(MemberCtx));
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*memberctx->r=memberctx->list;
			memberctx->list=FKL_VM_NIL;
		}
		else
			memberctx->list=memberctx->list->u.pair->cdr;
		fklNiResTp(stack);
	}
	fklNiPopTp(stack);
	fklNiReturn(*memberctx->r,&memberctx->ap,stack);
	fklNiEnd(&memberctx->ap,stack);
	free(ctx);
}

void builtin_member(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!obj||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.member",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.member",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.member",runnable,exe);
	if(proc)
	{
		FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.member",runnable,exe);
		MemberCtx* memberctx=(MemberCtx*)malloc(sizeof(MemberCtx));
		FKL_ASSERT(memberctx);
		fklPushVMvalue(FKL_VM_NIL,stack);
		memberctx->r=fklNiGetTopSlot(stack);
		memberctx->obj=obj;
		memberctx->proc=proc;
		memberctx->list=list;
		memberctx->ap=ap;
		k_member(exe,FKL_CC_OK,memberctx);
	}
	FklVMvalue* r=list;
	for(;r!=FKL_VM_NIL;r=r->u.pair->cdr)
		if(fklVMvalueEqual(r->u.pair->car,obj))
			break;
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

typedef struct
{
	FklVMvalue** r;
	FklVMvalue* proc;
	FklVMvalue* list;
	uint32_t ap;
}MempCtx;

static void k_memp(K_FUNC_ARGL)
{
	MempCtx* mempctx=(MempCtx*)ctx;
	FklVMstack* stack=exe->stack;
	if(s==FKL_CC_OK)
		fklNiSetTp(stack);
	else if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*mempctx->r=mempctx->list;
			mempctx->list=FKL_VM_NIL;
		}
		else
			mempctx->list=mempctx->list->u.pair->cdr;
		fklNiResTp(stack);
	}
	while(mempctx->list!=FKL_VM_NIL)
	{
		fklVMcallInDlproc(mempctx->proc
				,1,&mempctx->list->u.pair->car
				,exe->rhead,exe,k_memp,mempctx,sizeof(MempCtx));
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*mempctx->r=mempctx->list;
			mempctx->list=FKL_VM_NIL;
		}
		else
			mempctx->list=mempctx->list->u.pair->cdr;
		fklNiResTp(stack);
	}
	fklNiPopTp(stack);
	fklNiReturn(*mempctx->r,&mempctx->ap,stack);
	fklNiEnd(&mempctx->ap,stack);
	free(ctx);
}

void builtin_memp(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!proc||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memp",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memp",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.memp",runnable,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.memp",runnable,exe);
	MempCtx* mempctx=(MempCtx*)malloc(sizeof(MempCtx));
	FKL_ASSERT(mempctx);
	fklPushVMvalue(FKL_VM_NIL,stack);
	mempctx->r=fklNiGetTopSlot(stack);
	mempctx->proc=proc;
	mempctx->list=list;
	mempctx->ap=ap;
	k_memp(exe,FKL_CC_OK,mempctx);
}

typedef struct
{
	FklVMvalue** r;
	FklVMvalue** cur;
	FklVMvalue* proc;
	FklVMvalue* list;
	uint32_t ap;
}FilterCtx;

static void k_filter(K_FUNC_ARGL)
{
	FilterCtx* filterctx=(FilterCtx*)ctx;
	FklVMstack* stack=exe->stack;
	if(s==FKL_CC_OK)
		fklNiSetTp(stack);
	else if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*filterctx->cur=fklNewVMvalueToStack(FKL_TYPE_PAIR,fklNewVMpair(),stack,exe->gc);
			fklSetRef(&(*filterctx->cur)->u.pair->car,filterctx->list->u.pair->car,exe->gc);
			filterctx->cur=&(*filterctx->cur)->u.pair->cdr;
		}
		filterctx->list=filterctx->list->u.pair->cdr;
		fklNiResTp(stack);
	}
	while(filterctx->list!=FKL_VM_NIL)
	{
		fklVMcallInDlproc(filterctx->proc
				,1,&filterctx->list->u.pair->car
				,exe->rhead,exe,k_filter,filterctx,sizeof(FilterCtx));
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*filterctx->cur=fklNewVMvalueToStack(FKL_TYPE_PAIR,fklNewVMpair(),stack,exe->gc);
			fklSetRef(&(*filterctx->cur)->u.pair->car,filterctx->list->u.pair->car,exe->gc);
			filterctx->cur=&(*filterctx->cur)->u.pair->cdr;
		}
		filterctx->list=filterctx->list->u.pair->cdr;
		fklNiResTp(stack);
	}
	fklNiPopTp(stack);
	fklNiReturn(*filterctx->r,&filterctx->ap,stack);
	fklNiEnd(&filterctx->ap,stack);
	free(ctx);
}

void builtin_filter(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!proc||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.filter",FKL_ERR_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.filter",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.filter",runnable,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.filter",runnable,exe);
	FilterCtx* filterctx=(FilterCtx*)malloc(sizeof(FilterCtx));
	FKL_ASSERT(filterctx);
	fklPushVMvalue(FKL_VM_NIL,stack);
	filterctx->r=fklNiGetTopSlot(stack);
	filterctx->cur=filterctx->r;
	filterctx->proc=proc;
	filterctx->list=list;
	filterctx->ap=ap;
	k_filter(exe,FKL_CC_OK,filterctx);
}

void builtin_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvalue** r=fklNiGetTopSlot(stack);
	FklVMvalue** pcur=r;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		*pcur=fklNewVMpairV(cur,FKL_VM_NIL,stack,gc);
		pcur=&(*pcur)->u.pair->cdr;
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(*r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_reverse(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj)&&obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_WRONGARG,runnable,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(obj!=FKL_VM_NIL)
	{
		FklVMgc* gc=exe->gc;
		for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=cdr->u.pair->cdr)
			retval=fklNewVMpairV(cdr->u.pair->car,retval,stack,gc);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_feof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.feof",runnable,exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_INVALIDACCESS,runnable,exe);
	fklNiReturn(!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	size_t size=ap-stack->bp;
	FklVMvalue* vec=fklNewVMvecV(size,NULL,stack,exe->gc);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],fklNiGetArg(&ap,stack),exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(vec,&ap,stack);
}

void builtin_getdir(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getdir",FKL_ERR_TOOMANYARG,exe->rhead,exe);
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
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_STR,str,stack,exe->gc),&ap,stack);
	}
	else
	{
		const char* cwd=fklGetCwd();
		FklString* str=fklNewString(strlen(cwd),cwd);
		fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_STR,str,stack,exe->gc),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_fgetc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_WRONGARG,runnable,exe);
	FklVMfp* fp=stream?stream->u.fp:BuiltInStdin->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INVALIDACCESS,runnable,exe);
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

void builtin_fgeti(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_WRONGARG,runnable,exe);
	FklVMfp* fp=stream?stream->u.fp:BuiltInStdin->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INVALIDACCESS,runnable,exe);
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
			fklNiReturn(FKL_MAKE_VM_I32(ch),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_fwrite(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	if(FKL_IS_STR(obj)||FKL_IS_SYM(obj))
		fklPrincVMvalue(obj,objFile);
	else if(FKL_IS_BYTEVECTOR(obj))
		fwrite(obj->u.bvec->ptr,obj->u.bvec->size,1,objFile);
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
		obj->u.ud->t->__write(obj->u.ud->data,objFile);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_WRONGARG,runnable,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.box",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.box",FKL_ERR_TOOFEWARG,runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BOX,obj,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_unbox(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.unbox",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.unbox",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.unbox",runnable,exe);
	fklNiReturn(box->u.box,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-box!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj||!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-box!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.set-box!",runnable,exe);
	fklSetRef(&box->u.box,obj,exe->gc);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cas_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* new=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-box!",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!old||!box||!new)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-box!",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.cas-box!",runnable,exe);
	if(box->u.box==old)
	{
		fklSetRef(&box->u.box,new,exe->gc);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	size_t size=ap-stack->bp;
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(size,NULL),stack,gc);
	FklBytevector* bytevec=r->u.bvec;
	size_t i=0;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur!=NULL
			;cur=fklNiGetArg(&ap,stack),i++)
	{
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.bytevector",runnable,exe);
		bytevec->ptr[i]=fklGetInt(cur);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_TOOFEWARG,runnable,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-bytevector",runnable,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_TOOMANYARG,runnable,exe);
	size_t len=fklGetInt(size);
	FklVMvalue* r=fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(len,NULL),stack,gc);
	FklBytevector* bytevec=r->u.bvec;
	uint8_t u_8=0;
	if(content)
	{
		FKL_NI_CHECK_TYPE(content,fklIsInt,"builtin.make-bytevector",runnable,exe);
		u_8=fklGetInt(content);
	}
	memset(bytevec->ptr,u_8,len);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

#define PREDICATE(condtion,err_infor) FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!val)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(condtion)\
	fklNiReturn(FKL_VM_TRUE,&ap,stack);\
	else\
	fklNiReturn(FKL_VM_NIL,&ap,stack);\
	fklNiEnd(&ap,stack);

#ifndef _WIN32

static int getch()
{
	struct termios oldt,newt;
	int ch;
	tcgetattr(STDIN_FILENO,&oldt);
	newt=oldt;
	newt.c_lflag &=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&newt);
	ch=getchar();
	tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
	return ch;
}
#endif

void builtin_getch(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getch",FKL_ERR_TOOMANYARG,exe->rhead,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(getch()),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.sleep",r,exe);
	fklNiReturn(FKL_MAKE_VM_I32(sleep(fklGetInt(second))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_usleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=exe->rhead;
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.usleep",FKL_ERR_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.usleep",FKL_ERR_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.usleep",r,exe);
#ifdef _WIN32
		Sleep(fklGetInt(second));
#else
		usleep(fklGetInt(second));
#endif
	fklNiReturn(second,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_srand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* s=fklNiGetArg(&ap,stack);
    if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.srand",FKL_ERR_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(s,fklIsInt,"builtin.srand",r,exe);
    srand(fklGetInt(s));
    fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_rand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue*  lim=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_TOOMANYARG,r,exe);
	if(lim&&!fklIsInt(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_WRONGARG,r,exe);
	fklNiReturn(FKL_MAKE_VM_I32(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_get_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get-time",FKL_ERR_TOOMANYARG,exe->rhead,exe);
	time_t timer=time(NULL);
	struct tm* tblock=NULL;
	tblock=localtime(&timer);
	char sec[4]={0};
	char min[4]={0};
	char hour[4]={0};
	char day[4]={0};
	char mon[4]={0};
	char year[10]={0};
	snprintf(sec,4,"%u",tblock->tm_sec);
	snprintf(min,4,"%u",tblock->tm_min);
	snprintf(hour,4,"%u",tblock->tm_hour);
	snprintf(day,4,"%u",tblock->tm_mday);
	snprintf(mon,4,"%u",tblock->tm_mon+1);
	snprintf(year,10,"%u",tblock->tm_year+1900);
	uint32_t timeLen=strlen(year)+strlen(mon)+strlen(day)+strlen(hour)+strlen(min)+strlen(sec)+5+1;
	char* trueTime=(char*)malloc(sizeof(char)*timeLen);
	FKL_ASSERT(trueTime);
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	FklString* str=fklNewString(timeLen-1,trueTime);
	FklVMvalue* tmpVMvalue=fklNewVMvalueToStack(FKL_TYPE_STR,str,stack,exe->gc);
	free(trueTime);
	fklNiReturn(tmpVMvalue,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_remove_file(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=exe->rhead;
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.remove-file",FKL_ERR_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.remove-file",FKL_ERR_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"builtin.remove-file",r,exe);
	char str[name->u.str->size+1];
	fklWriteStringToCstr(str,name->u.str);
	fklNiReturn(FKL_MAKE_VM_I32(remove(str)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.time",FKL_ERR_TOOMANYARG,r,exe);
	fklNiReturn(fklMakeVMint((int64_t)time(NULL),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* sym=fklNiGetArg(&ap,stack);
	FklVMvalue* value=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set!",FKL_ERR_TOOMANYARG,r,exe);
	if(!sym||!value)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set!",FKL_ERR_TOOFEWARG,r,exe);
	FKL_NI_CHECK_TYPE(sym,FKL_IS_SYM,"builtin.set!",r,exe);
	fklSetRef(fklFindOrAddVar(FKL_GET_SYM(sym),r->localenv->u.env)
			,value
			,exe->gc);
	fklNiReturn(value,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_system(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklNiGetArg(&ap,stack);
	FklVMrunnable* r=exe->rhead;
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.system",FKL_ERR_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.system",FKL_ERR_TOOMANYARG,r,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"builtin.system",r,exe);
	char str[name->u.str->size+1];
	fklWriteStringToCstr(str,name->u.str);
	fklNiReturn(fklMakeVMint(system(str),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_not(ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.not")}
void builtin_null(ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.null")}
void builtin_atom(ARGL) {PREDICATE(!FKL_IS_PAIR(val),"builtin.atom?")}
void builtin_char_p(ARGL) {PREDICATE(FKL_IS_CHR(val),"builtin.char?")}
void builtin_integer_p(ARGL) {PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val)||FKL_IS_BIG_INT(val),"builtin.integer?")}
void builtin_fix_int_p(ARGL) {PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val),"builtin.fix-int?")}
void builtin_i32_p(ARGL) {PREDICATE(FKL_IS_I32(val),"builtin.i32?")}
void builtin_i64_p(ARGL) {PREDICATE(FKL_IS_I64(val),"builtin.i64?")}
void builtin_f64_p(ARGL) {PREDICATE(FKL_IS_F64(val),"builtin.i64?")}
void builtin_number_p(ARGL) {PREDICATE(fklIsVMnumber(val),"builtin.number?")}
void builtin_pair_p(ARGL) {PREDICATE(FKL_IS_PAIR(val),"builtin.pair?")}
void builtin_symbol_p(ARGL) {PREDICATE(FKL_IS_SYM(val),"builtin.symbol?")}
void builtin_string_p(ARGL) {PREDICATE(FKL_IS_STR(val),"builtin.string?")}
void builtin_error_p(ARGL) {PREDICATE(FKL_IS_ERR(val),"builtin.error?")}
void builtin_procedure_p(ARGL) {PREDICATE(FKL_IS_PROC(val)||FKL_IS_DLPROC(val),"builtin.procedure?")}
void builtin_proc_p(ARGL) {PREDICATE(FKL_IS_PROC(val),"builtin.proc?")}
void builtin_dlproc_p(ARGL) {PREDICATE(FKL_IS_DLPROC(val),"builtin.dlproc?")}
void builtin_vector_p(ARGL) {PREDICATE(FKL_IS_VECTOR(val),"builtin.vector?")}
void builtin_bytevector_p(ARGL) {PREDICATE(FKL_IS_BYTEVECTOR(val),"builtin.bytevector?")}
void builtin_chanl_p(ARGL) {PREDICATE(FKL_IS_CHAN(val),"builtin.chanl?")}
void builtin_dll_p(ARGL) {PREDICATE(FKL_IS_DLL(val),"builtin.dll?")}
void builtin_big_int_p(ARGL) {PREDICATE(FKL_IS_BIG_INT(val),"builtin.big-int?")}
void builtin_list_p(ARGL){PREDICATE(fklIsList(val),"builtin.list?")}
void builtin_box_p(ARGL) {PREDICATE(FKL_IS_BOX(val),"builtin.box?")}

#undef ARGL
#undef K_FUNC_ARGL
#undef PREDICATE
//end

static const struct SymbolFuncStruct
{
	const char* s;
	FklVMdllFunc f;
}builtInSymbolList[]=
{
	{"nil",                   NULL,                            },
	{"stdin",                 NULL,                            },
	{"stdout",                NULL,                            },
	{"stderr",                NULL,                            },
	{"car",                   builtin_car,                     },
	{"cdr",                   builtin_cdr,                     },
	{"cons",                  builtin_cons,                    },
	{"append",                builtin_append,                  },
	{"copy",                  builtin_copy,                    },
	{"atom",                  builtin_atom,                    },
	{"null",                  builtin_null,                    },
	{"not",                   builtin_not,                     },
	{"eq",                    builtin_eq,                      },
	{"eqv",                   builtin_eqv,                     },
	{"equal",                 builtin_equal,                   },
	{"=",                     builtin_eqn,                     },
	{"+",                     builtin_add,                     },
	{"1+",                    builtin_add_1,                   },
	{"-",                     builtin_sub,                     },
	{"-1+",                   builtin_sub_1,                   },
	{"*",                     builtin_mul,                     },
	{"/",                     builtin_div,                     },
	{"//",                    builtin_idiv,                    },
	{"%",                     builtin_mod,                     },
	{"abs",                   builtin_abs,                     },
	{">",                     builtin_gt,                      },
	{">=",                    builtin_ge,                      },
	{"<",                     builtin_lt,                      },
	{"<=",                    builtin_le,                      },
	{"nth",                   builtin_nth,                     },
	{"length",                builtin_length,                  },
	{"apply",                 builtin_apply,                   },
	{"call/cc",               builtin_call_cc,                 },
	{"fopen",                 builtin_fopen,                   },
	{"read",                  builtin_read,                    },
	{"prin1",                 builtin_prin1,                   },
	{"princ",                 builtin_princ,                   },
	{"dlopen",                builtin_dlopen,                  },
	{"dlsym",                 builtin_dlsym,                   },
	{"argv",                  builtin_argv,                    },
	{"go",                    builtin_go,                      },
	{"chanl",                 builtin_chanl,                   },
	{"send",                  builtin_send,                    },
	{"recv",                  builtin_recv,                    },
	{"error",                 builtin_error,                   },
	{"raise",                 builtin_raise,                   },
	{"reverse",               builtin_reverse,                 },
	{"fclose",                builtin_fclose,                  },
	{"feof",                  builtin_feof,                    },
	{"nthcdr",                builtin_nthcdr,                  },
	{"char?",                 builtin_char_p,                  },
	{"integer?",              builtin_integer_p,               },
	{"i32?",                  builtin_i32_p,                   },
	{"i64?",                  builtin_i64_p,                   },
	{"f64?",                  builtin_f64_p,                   },
	{"pair?",                 builtin_pair_p,                  },

	{"symbol?",               builtin_symbol_p,                },
	{"string->symbol",        builtin_string_to_symbol,        },

	{"string?",               builtin_string_p,                },
	{"string",                builtin_string,                  },
	{"sub-string",            builtin_sub_string,              },
	{"make-string",           builtin_make_string,             },
	{"symbol->string",        builtin_symbol_to_string,        },
	{"number->string",        builtin_number_to_string,        },
	{"vector->string",        builtin_vector_to_string,        },
	{"bytevector->string",    builtin_bytevector_to_string,    },
	{"list->string",          builtin_list_to_string,          },
	{"->string",              builtin_to_string,               },
	{"sref",                  builtin_sref,                    },
	{"set-sref!",             builtin_set_sref,                },
	{"fill-string!",          builtin_fill_string,             },

	{"error?",                builtin_error_p,                 },
	{"procedure?",            builtin_procedure_p,             },
	{"proc?",                 builtin_proc_p,                  },
	{"dlproc?",               builtin_dlproc_p,                },

	{"vector?",               builtin_vector_p,                },
	{"vector",                builtin_vector,                  },
	{"make-vector",           builtin_make_vector,             },
	{"sub-vector",            builtin_sub_vector,              },
	{"list->vector",          builtin_list_to_vector,          },
	{"string->vector",        builtin_string_to_vector,        },
	{"vref",                  builtin_vref,                    },
	{"set-vref!",             builtin_set_vref,                },
	{"cas-vref!",             builtin_cas_vref,                },
	{"fill-vector!",          builtin_fill_vector,             },

	{"list?",                 builtin_list_p,                  },
	{"list",                  builtin_list,                    },
	{"make-list",             builtin_make_list,               },
	{"vector->list",          builtin_vector_to_list,          },
	{"string->list",          builtin_string_to_list,          },
	{"set-nth!",              builtin_set_nth,                 },
	{"set-nthcdr!",           builtin_set_nthcdr,              },

	{"bytevector?",           builtin_bytevector_p,            },
	{"bytevector",            builtin_bytevector,              },
	{"sub-bytevector",        builtin_sub_bytevector,          },
	{"make-bytevector",       builtin_make_bytevector,         },
	{"bytevector->s8-list",   builtin_bytevector_to_s8_list,   },
	{"bytevector->u8-list",   builtin_bytevector_to_u8_list,   },
	{"bytevector->s8-vector", builtin_bytevector_to_s8_vector, },
	{"bytevector->u8-vector", builtin_bytevector_to_u8_vector, },

	{"bvs8ref",               builtin_bvs8ref,                 },
	{"bvs16ref",              builtin_bvs16ref,                },
	{"bvs32ref",              builtin_bvs32ref,                },
	{"bvs64ref",              builtin_bvs64ref,                },
	{"bvu8ref",               builtin_bvu8ref,                 },
	{"bvu16ref",              builtin_bvu16ref,                },
	{"bvu32ref",              builtin_bvu32ref,                },
	{"bvu64ref",              builtin_bvu64ref,                },
	{"bvf32ref",              builtin_bvf32ref,                },
	{"bvf64ref",              builtin_bvf64ref,                },
	{"set-bvs8ref!",          builtin_set_bvs8ref,             },
	{"set-bvs16ref!",         builtin_set_bvs16ref,            },
	{"set-bvs32ref!",         builtin_set_bvs32ref,            },
	{"set-bvs64ref!",         builtin_set_bvs64ref,            },
	{"set-bvu8ref!",          builtin_set_bvu8ref,             },
	{"set-bvu16ref!",         builtin_set_bvu16ref,            },
	{"set-bvu32ref!",         builtin_set_bvu32ref,            },
	{"set-bvu64ref!",         builtin_set_bvu64ref,            },
	{"set-bvf32ref!",         builtin_set_bvf32ref,            },
	{"set-bvf64ref!",         builtin_set_bvf64ref,            },
	{"fill-bytevector!",      builtin_fill_bytevector,         },

	{"chanl?",                builtin_chanl_p,                 },
	{"dll?",                  builtin_dll_p,                   },
	{"getdir",                builtin_getdir,                  },
	{"fgetc",                 builtin_fgetc,                   },
	{"fgeti",                 builtin_fgeti,                   },
	{"fwrite",                builtin_fwrite,                  },
	{"fgets",                 builtin_fgets,                   },
	{"fgetb",                 builtin_fgetb,                   },

	{"big-int?",              builtin_big_int_p,               },

	{"set-car!",              builtin_set_car,                 },
	{"set-cdr!",              builtin_set_cdr,                 },
	{"box",                   builtin_box,                     },
	{"unbox",                 builtin_unbox,                   },
	{"set-box!",              builtin_set_box,                 },
	{"cas-box!",              builtin_cas_box,                 },
	{"box?",                  builtin_box_p,                   },
	{"fix-int?",              builtin_fix_int_p,               },

	{"number?",               builtin_number_p,                },
	{"string->number",        builtin_string_to_number,        },
	{"char->integer",         builtin_char_to_integer,         },
	{"symbol->integer",       builtin_symbol_to_integer,       },
	{"integer->char",         builtin_integer_to_char,         },
	{"number->f64",           builtin_number_to_f64,           },
	{"number->integer",       builtin_number_to_integer,       },
	{"number->i32",           builtin_number_to_i32,           },
	{"number->i64",           builtin_number_to_i64,           },
	{"number->big-int",       builtin_number_to_big_int,       },

	{"map",                   builtin_map,                     },
	{"foreach",               builtin_foreach,                 },
	{"andmap",                builtin_andmap,                  },
	{"ormap",                 builtin_ormap,                   },
	{"memq",                  builtin_memq,                    },
	{"member",                builtin_member,                  },
	{"memp",                  builtin_memp,                    },
	{"filter",                builtin_filter,                  },

	{"set!",                  builtin_set,                     },

	{"getch",                 builtin_getch,                   },
	{"sleep",                 builtin_sleep,                   },
	{"usleep",                builtin_usleep,                  },
	{"srand",                 builtin_srand,                   },
	{"rand",                  builtin_rand,                    },
	{"get-time",              builtin_get_time,                },
	{"remove-file",           builtin_remove_file,             },
	{"time",                  builtin_time,                    },
	{"system",                builtin_system,                  },

//	{"hash",builtin_hash,},

	{NULL,                    NULL,                            },
};

void fklInitCompEnv(FklCompEnv* curEnv)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddCompDefCstr(list->s,curEnv);
}

void fklInitGlobEnv(FklVMenv* obj,FklVMgc* gc)
{
	const struct SymbolFuncStruct* list=builtInSymbolList;
	BuiltInStdin=fklNewVMvalueNoGC(FKL_TYPE_FP,fklNewVMfp(stdin),gc);
	BuiltInStdout=fklNewVMvalueNoGC(FKL_TYPE_FP,fklNewVMfp(stdout),gc);
	BuiltInStderr=fklNewVMvalueNoGC(FKL_TYPE_FP,fklNewVMfp(stderr),gc);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,FKL_VM_NIL,obj);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,BuiltInStdin,obj);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,BuiltInStdout,obj);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,BuiltInStderr,obj);
	for(;list->s!=NULL;list++)
	{
		FklVMdlproc* proc=fklNewVMdlproc(list->f,NULL);
		FklSymTabNode* node=fklAddSymbolToGlobCstr(list->s);
		proc->sid=node->id;
		fklFindOrAddVarWithValue(node->id,fklNewVMvalueNoGC(FKL_TYPE_DLPROC,proc,gc),obj);
	}
}
