#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/builtin.h>
#include<fakeLisp/codegen.h>
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

static FklSid_t builtInHeadSymbolTable[4]={0};

static const char* builtInErrorType[]=
{
	"dummy",
	"symbol-undefined",
	"syntax-error",
	"invalid-expression",
	"circular-load",
	"invalid-pattern",
	"incorrect-types-of-values",
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
	"invalid-value",
	"invalid-assign",
	"invalid-access",
	"import-failed",
	"invalid-macro-pattern",
	"faild-to-create-big-int-from-mem",
	"differ-list-in-list",
	"cross-c-call-continuation",
	"invalid-radix",
	"no-value-for-key",
	"number-should-not-be-less-than-0",
	NULL,
};

FklSid_t fklGetBuiltInErrorType(FklBuiltInErrorType type)
{
	static FklSid_t errorTypeId[sizeof(builtInErrorType)/sizeof(const char*)]={0};
	if(!errorTypeId[type])
		errorTypeId[type]=fklAddSymbolToGlobCstr(builtInErrorType[type])->id;
	return errorTypeId[type];
}

extern void applyNativeProc(FklVM*,FklVMproc*,FklVMframe*);
extern void* ThreadVMfunc(void* p);

//builtin functions

#define ARGL FklVM* exe
#define K_FUNC_ARGL FklVM* exe,FklCCState s,void* ctx
void builtin_car(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.car",frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.car",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.car",frame,exe);
	fklNiReturn(obj->u.pair->car,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_car(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.set-car!",frame,exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-car!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.set-car!",frame,exe);
	fklSetRef(&obj->u.pair->car,target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.cdr",frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cdr",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.cdr",frame,exe);
	fklNiReturn(obj->u.pair->cdr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.set-cdr!",frame,exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-cdr!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.set-cdr",frame,exe);
	fklSetRef(&obj->u.pair->cdr,target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cons(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.cons",frame,exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cons",FKL_ERR_TOOFEWARG,frame,exe);
	fklNiReturn(fklCreateVMpairV(car,cdr,exe),&ap,stack);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_TOOMANYARG,frame,exe);
	FklVMvalue* retval=fklCopyVMvalue(obj,exe);
	if(!retval)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_append(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=fklNiGetArg(&ap,stack);
		if(cur)
			retval=fklCopyVMvalue(retval,exe);
		if(!retval)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
				*prev=fklCopyVMlistOrAtom(*prev,exe);
				for(;FKL_IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		}
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_eq(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOFEWARG,frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOFEWARG,frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.equal",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.equal",FKL_ERR_TOOFEWARG,frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt* bi=fklCreateBigInt0();
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
			fklDestroyBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.+",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=0.0)
	{
		rd+=r64+fklBigIntToDouble(bi);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		fklDestroyBigInt(bi);
	}
	else if(FKL_IS_0_BIG_INT(bi))
	{
		fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
		fklDestroyBigInt(bi);
	}
	else
	{
		fklAddBigIntI(bi,r64);
		if(fklIsGtLtI64BigInt(bi))
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),exe),&ap,stack);
			fklDestroyBigInt(bi);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_add_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_TOOMANYARG,frame,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.1+",frame,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else
	{
		if(FKL_IS_BIG_INT(arg))
		{
			if(fklIsGeLeI64BigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklCreateBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklBigIntToI64(arg->u.bigInt)+1,exe),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==INT64_MAX)
			{
				FklBigInt* bi=fklCreateBigInt(i);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklGetInt(arg)+1,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_sub(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"builtin.-",frame,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		}
		else if(fklIsFixint(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64MulOverflow(p64,-1))
			{
				FklBigInt* bi=fklCreateBigInt(p64);
				fklMulBigIntI(bi,-1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(-fklGetInt(prev),exe),&ap,stack);
		}
		else
		{
			FklBigInt* bi=fklCreateBigInt0();
			fklSetBigInt(bi,prev->u.bigInt);
			fklMulBigIntI(bi,-1);
			if(fklIsGtLtI64BigInt(bi))
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			else
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(bi),exe),&ap,stack);
				fklDestroyBigInt(bi);
			}
		}
	}
	else
	{
		FklBigInt* bi=fklCreateBigInt0();
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
				fklDestroyBigInt(bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=fklGetDouble(prev)-rd-r64-fklBigIntToDouble(bi);
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
			fklDestroyBigInt(bi);
		}
		else if(FKL_IS_0_BIG_INT(bi)&&!FKL_IS_BIG_INT(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64AddOverflow(p64,-r64))
			{
				fklAddBigIntI(bi,p64);
				fklSubBigIntI(bi,r64);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
			{
				r64=p64-r64;
				fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
				fklDestroyBigInt(bi);
			}
		}
		else
		{
			fklSubBigInt(bi,prev->u.bigInt);
			fklMulBigIntI(bi,-1);
			fklSubBigIntI(bi,r64);
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_abs(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.abs",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.abs",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.abs",frame,exe);
	if(FKL_IS_F64(obj))
		fklNiReturn(fklMakeVMf64(fabs(obj->u.f64),exe),&ap,stack);
	else
	{
		if(fklIsFixint(obj))
		{
			int64_t i=fklGetInt(obj);
			if(i==INT64_MIN)
			{
				FklBigInt* bi=fklCreateBigInt(i);
				bi->neg=0;
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMuint(labs(i),exe),&ap,stack);
		}
		else
		{
			FklBigInt* bi=fklCopyBigInt(obj->u.bigInt);
			bi->neg=0;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_sub_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_TOOMANYARG,frame,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.-1+",frame,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else
	{
		if(FKL_IS_BIG_INT(arg))
		{
			if(fklIsGeLeI64BigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklCreateBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(fklBigIntToI64(arg->u.bigInt)-1,exe),&ap,stack);
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
				fklNiReturn(fklMakeVMint(fklGetInt(arg)-1,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_mul(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt* bi=fklCreateBigInt1();
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
			fklDestroyBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.*",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=1.0)
	{
		rd*=r64*fklBigIntToDouble(bi);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		fklDestroyBigInt(bi);
	}
	else if(FKL_IS_1_BIG_INT(bi))
	{
		fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
		fklDestroyBigInt(bi);
	}
	else
	{
		fklMulBigIntI(bi,r64);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_idiv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsInt,"builtin.//",frame,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(fklIsFixint(prev))
		{
			r64=fklGetInt(prev);
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,frame,exe);
			if(r64==1)
				fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
			else
				fklNiReturn(FKL_MAKE_VM_I32(0),&ap,stack);
		}
		else
		{
			if(FKL_IS_0_BIG_INT(prev->u.bigInt))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,frame,exe);
			if(FKL_IS_1_BIG_INT(prev->u.bigInt))
				fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
			else
				fklNiReturn(FKL_MAKE_VM_I32(0),&ap,stack);
		}
	}
	else
	{
		FklBigInt* bi=fklCreateBigInt1();
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
				fklDestroyBigInt(bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi))
		{
			fklDestroyBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,frame,exe);
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_BIG_INT(prev)&&!fklIsGtLtI64BigInt(prev->u.bigInt)&&!FKL_IS_1_BIG_INT(bi))
		{
			FklBigInt* t=fklCreateBigInt0();
			fklSetBigInt(t,prev->u.bigInt);
			fklDivBigInt(t,bi);
			fklDivBigIntI(t,r64);
			if(fklIsGtLtI64BigInt(t))
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,t,exe),&ap,stack);
			else
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(t),exe),&ap,stack);
				fklDestroyBigInt(t);
			}
		}
		else
		{
			r64=fklGetInt(prev)/r64;
			fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
		}
		fklDestroyBigInt(bi);
	}
	fklNiEnd(&ap,stack);
}

void builtin_div(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"builtin./",frame,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,frame,exe);
			rd=1/prev->u.f64;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		}
		else
		{
			if(fklIsFixint(prev))
			{
				r64=fklGetInt(prev);
				if(!r64)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,frame,exe);
				if(1%r64)
				{
					rd=1.0/r64;
					fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
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
					fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
				}
			}
		}
	}
	else
	{
		FklBigInt* bi=fklCreateBigInt1();
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
				fklDestroyBigInt(bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi)||rd==0.0)
		{
			fklDestroyBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,frame,exe);
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
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		}
		else
		{
			if(FKL_IS_BIG_INT(prev)&&!fklIsGtLtI64BigInt(prev->u.bigInt)&&!FKL_IS_1_BIG_INT(bi))
			{
				FklBigInt* t=fklCreateBigInt0();
				fklSetBigInt(t,prev->u.bigInt);
				fklDivBigInt(t,bi);
				fklDivBigIntI(t,r64);
				if(fklIsGtLtI64BigInt(t))
					fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,t,exe),&ap,stack);
				else
				{
					fklNiReturn(fklMakeVMint(fklBigIntToI64(t),exe),&ap,stack);
					fklDestroyBigInt(t);
				}
			}
			else
			{
				r64=fklGetInt(prev)/r64;
				fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
			}
		}
		fklDestroyBigInt(bi);
	}
	fklNiEnd(&ap,stack);
}

void builtin_mod(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=fklGetDouble(fir);
		double as=fklGetDouble(sec);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,frame,exe);
		double r=fmod(af,as);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else if(fklIsFixint(fir)&&fklIsFixint(sec))
	{
		int64_t si=fklGetInt(sec);
		int64_t r=fklGetInt(fir)%si;
		if(si==0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,frame,exe);
		fklNiReturn(fklMakeVMint(r,exe),&ap,stack);
	}
	else
	{
		FklBigInt* rem=fklCreateBigInt0();
		if(FKL_IS_BIG_INT(fir))
			fklSetBigInt(rem,fir->u.bigInt);
		else
			fklSetBigIntI(rem,fklGetInt(fir));
		if(FKL_IS_BIG_INT(sec))
		{
			if(FKL_IS_0_BIG_INT(sec->u.bigInt))
			{
				fklDestroyBigInt(rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,frame,exe);
			}
			fklModBigInt(rem,sec->u.bigInt);
		}
		else
		{
			int64_t si=fklGetInt(sec);
			if(si==0)
			{
				fklDestroyBigInt(rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,frame,exe);
			}
			fklModBigIntI(rem,si);
		}
		if(fklIsGtLtI64BigInt(rem))
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,rem,exe),&ap,stack);
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(rem),exe),&ap,stack);
			fklDestroyBigInt(rem);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_eqn(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_TOOFEWARG,frame,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)==0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
	FklVMframe* frame=exe->frames;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_TOOFEWARG,frame,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
	FklVMframe* frame=exe->frames;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_TOOFEWARG,frame,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
	FklVMframe* frame=exe->frames;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_TOOFEWARG,frame,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
	FklVMframe* frame=exe->frames;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_TOOFEWARG,frame,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_CHR,"builtin.char->integer",frame,exe);
	fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_integer_to_char(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->char",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->char",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsInt,"builtin.integer->char",frame,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(fklGetInt(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->vector",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->vector",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsList,"builtin.list->vector",frame,exe);
	size_t len=fklVMlistLength(obj);
	FklVMvalue* r=fklCreateVMvecV(len,NULL,exe);
	for(size_t i=0;obj!=FKL_VM_NIL;i++,obj=obj->u.pair->cdr)
		fklSetRef(&r->u.vec->base[i],obj->u.pair->car,exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->vector",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->vector",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->vector",frame,exe);
	size_t len=obj->u.str->size;
	FklVMvalue* r=fklCreateVMvecV(len,NULL,exe);
	for(size_t i=0;i<len;i++)
		fklSetRef(&r->u.vec->base[i],FKL_MAKE_VM_CHR(obj->u.str->str[i]),exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMframe* frame=exe->frames;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-list",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-list",frame,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-list",FKL_ERR_TOOMANYARG,frame,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-list",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,frame,exe);
	size_t len=fklGetUint(size);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	FklVMvalue* t=content?content:FKL_VM_NIL;
	for(size_t i=0;i<len;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->list",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->list",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->list",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklString* str=obj->u.str;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<str->size;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-list",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	int8_t* s8a=(int8_t*)obj->u.bvec->ptr;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-list",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	uint8_t* u8a=obj->u.bvec->ptr;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-vector",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-vector",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-vector",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	int8_t* s8a=(int8_t*)obj->u.bvec->ptr;
	FklVMvalue* vec=fklCreateVMvecV(obj->u.bvec->size,NULL,exe);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_I32(s8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_u8_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->u8-vector",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->u8-vector",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->u8-vector",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	size_t size=obj->u.bvec->size;
	uint8_t* u8a=obj->u.bvec->ptr;
	FklVMvalue* vec=fklCreateVMvecV(obj->u.bvec->size,NULL,exe);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_I32(u8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->list",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->list",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_VECTOR,"builtin.vector->list",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvec* vec=obj->u.vec;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(size_t i=0;i<vec->size;i++)
	{
		fklSetRef(cur,fklCreateVMpairV(vec->base[i],FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
		fklPopVMstack(stack);
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	size_t size=ap-stack->bp;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
	FklString* str=r->u.str;
	size_t i=0;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur!=NULL
			;cur=fklNiGetArg(&ap,stack),i++)
	{
		FKL_NI_CHECK_TYPE(cur,FKL_IS_CHR,"builtin.string",frame,exe);
		str->str[i]=FKL_GET_CHR(cur);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-string",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-string",frame,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-string",FKL_ERR_TOOMANYARG,frame,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(len,NULL),exe);
	FklString* str=r->u.str;
	char ch=0;
	if(content)
	{
		FKL_NI_CHECK_TYPE(content,FKL_IS_CHR,"builtin.make-string",frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-vector",frame,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOMANYARG,frame,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,frame,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_VECTOR,fklCreateVMvec(len),exe);
	if(content)
	{
		FklVMvec* vec=r->u.vec;
		for(size_t i=0;i<len;i++)
			fklSetRef(&vec->base[i],content,gc);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_substring(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.substring",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.substring",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_STR,"builtin.substring",frame,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.substring",frame,exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.substring",frame,exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.substring",FKL_ERR_INVALIDACCESS,frame,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,ostr->u.str->str+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vsize=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vsize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_STR,"builtin.sub-string",frame,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-string",frame,exe);
	FKL_NI_CHECK_TYPE(vsize,fklIsInt,"builtin.sub-string",frame,exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_INVALIDACCESS,frame,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,ostr->u.str->str+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_subbytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subbytevector",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subbytevector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,"builtin.subbytevector",frame,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.subbytevector",frame,exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.subbytevector",frame,exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subbytevector",FKL_ERR_INVALIDACCESS,frame,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,ostr->u.bvec->ptr+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vsize=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vsize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,"builtin.sub-bytevector",frame,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-bytevector",frame,exe);
	FKL_NI_CHECK_TYPE(vsize,fklIsInt,"builtin.sub-bytevector",frame,exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_INVALIDACCESS,frame,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,ostr->u.bvec->ptr+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_subvector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ovec=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ovec||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subvector",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subvector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(ovec,FKL_IS_VECTOR,"builtin.subvector",frame,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.subvector",frame,exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.subvector",frame,exe);
	size_t size=ovec->u.vec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subvector",FKL_ERR_INVALIDACCESS,frame,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvecV(size,ovec->u.vec->base+start,exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ovec=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vsize=fklNiGetArg(&ap,stack);
	if(!ovec||!vstart||!vsize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(ovec,FKL_IS_VECTOR,"builtin.sub-vector",frame,exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-vector",frame,exe);
	FKL_NI_CHECK_TYPE(vsize,fklIsInt,"builtin.sub-vector",frame,exe);
	size_t size=ovec->u.vec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_INVALIDACCESS,frame,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvecV(size,ovec->u.vec->base+start,exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_TOOFEWARG,frame,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(FKL_IS_SYM(obj))
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR
				,fklCopyString(fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol)
				,exe);
	else if(FKL_IS_CHR(obj))
	{
		FklString* r=fklCreateString(1,NULL);
		r->str[0]=FKL_GET_CHR(obj);
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR
				,r
				,exe);
	}
	else if(fklIsVMnumber(obj))
	{
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,NULL,exe);
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
			retval->u.str=fklCreateString(size,buf);
		}
	}
	else if(FKL_IS_BYTEVECTOR(obj))
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR
				,fklCreateString(obj->u.bvec->size,(char*)obj->u.bvec->ptr)
				,exe);
	else if(FKL_IS_VECTOR(obj))
	{
		size_t size=obj->u.vec->size;
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
		for(size_t i=0;i<size;i++)
		{
			FKL_NI_CHECK_TYPE(obj->u.vec->base[i],FKL_IS_CHR,"builtin.->string",frame,exe);
			retval->u.str->str[i]=FKL_GET_CHR(obj->u.vec->base[i]);
		}
	}
	else if(fklIsList(obj))
	{
		size_t size=fklVMlistLength(obj);
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
		for(size_t i=0;i<size;i++)
		{
			FKL_NI_CHECK_TYPE(obj->u.pair->car,FKL_IS_CHR,"builtin.->string",frame,exe);
			retval->u.str->str[i]=FKL_GET_CHR(obj->u.pair->car);
			obj=obj->u.pair->cdr;
		}
	}
	else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__to_string)
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,obj->u.ud->t->__to_string(obj->u.ud->data),exe);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_symbol_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->string",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->string",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_SYM,"builtin.symbol->string",frame,exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR
			,fklCopyString(fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol)
			,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_symbol(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->symbol",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->symbol",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->symbol",frame,exe);
	fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbolToGlob(obj->u.str)->id),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_symbol_to_integer(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->integer",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->integer",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_SYM,"builtin.symbol->integer",frame,exe);
	FklVMvalue* r=fklMakeVMint(FKL_GET_SYM(obj),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_number(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->number",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->number",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->number",frame,exe);
	FklVMvalue* r=FKL_VM_NIL;
	if(fklIsNumberString(obj->u.str))
	{
		if(fklIsDoubleString(obj->u.str))
		{
			double d=fklStringToDouble(obj->u.str);
			r=fklCreateVMvalueToStack(FKL_TYPE_F64,&d,exe);
		}
		else
		{
			FklBigInt* bi=fklCreateBigIntFromString(obj->u.str);
			if(!fklIsGtLtI64BigInt(bi))
			{
				r=fklMakeVMint(fklBigIntToI64(bi),exe);
				fklDestroyBigInt(bi);
			}
			else
				r=fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe);
		}
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* radix=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->string",frame,exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR,NULL,exe);
	if(fklIsInt(obj))
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_NI_CHECK_TYPE(radix,fklIsInt,"builtin.number->string",frame,exe);
			int64_t t=fklGetInt(radix);
			if(t!=8&&t!=10&&t!=16)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_INVALIDRADIX,frame,exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOMANYARG,frame,exe);
		size_t size=snprintf(buf,64,"%lf",obj->u.f64);
		retval->u.str=fklCreateString(size,buf);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->string",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.vector->string",frame,exe);
	size_t size=vec->u.vec->size;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
	for(size_t i=0;i<size;i++)
	{
		FKL_NI_CHECK_TYPE(vec->u.vec->base[i],FKL_IS_CHR,"builtin.vector->string",frame,exe);
		r->u.str->str[i]=FKL_GET_CHR(vec->u.vec->base[i]);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->string",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_BYTEVECTOR,"builtin.bytevector->string",frame,exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(vec->u.bvec->size,(char*)vec->u.bvec->ptr),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->bytevector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(str,FKL_IS_STR,"builtin.string->bytevector",frame,exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(str->u.str->size,(uint8_t*)str->u.str->str),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->bytevector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.vector->bytevector",frame,exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(vec->u.str->size,NULL),exe);
	uint64_t size=vec->u.vec->size;
	FklVMvalue** base=vec->u.vec->base;
	uint8_t* ptr=r->u.bvec->ptr;
	for(uint64_t i=0;i<size;i++)
	{
		FklVMvalue* cur=base[i];
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.vector->bytevector",frame,exe);
		ptr[i]=fklGetInt(cur);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->bytevector",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.list->bytevector",frame,exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(fklVMlistLength(list),NULL),exe);
	uint8_t* ptr=r->u.bvec->ptr;
	for(size_t i=0;list!=FKL_VM_NIL;i++,list=list->u.pair->cdr)
	{
		FklVMvalue* cur=list->u.pair->car;
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.list->bytevector",frame,exe);
		ptr[i]=fklGetInt(cur);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->string",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.list->string",frame,exe);
	size_t size=fklVMlistLength(list);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
	for(size_t i=0;i<size;i++)
	{
		FKL_NI_CHECK_TYPE(list->u.pair->car,FKL_IS_CHR,"builtin.list->string",frame,exe);
		r->u.str->str[i]=FKL_GET_CHR(list->u.pair->car);
		list=list->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_f64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOFEWARG,frame,exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_F64,NULL,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.integer->f64",frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->integer",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->integer",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->integer",frame,exe);
	if(FKL_IS_F64(obj))
	{
		if(obj->u.f64-(double)INT64_MAX>DBL_EPSILON||obj->u.f64-INT64_MIN<-DBL_EPSILON)
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCreateBigIntD(obj->u.f64),exe),&ap,stack);
		else
			fklNiReturn(fklMakeVMintD(obj->u.f64,exe),&ap,stack);
	}
	else if(FKL_IS_BIG_INT(obj))
	{
		if(fklIsGtLtI64BigInt(obj->u.bigInt))
		{
			FklBigInt* bi=fklCreateBigInt0();
			fklSetBigInt(bi,obj->u.bigInt);
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
		}
		else
			fklNiReturn(fklMakeVMint(fklBigIntToI64(obj->u.bigInt),exe),&ap,stack);
	}
	else
		fklNiReturn(fklMakeVMint(fklGetInt(obj),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_i32(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i32",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i32",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->i32",frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->big-int",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->big-int",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->big-int",frame,exe);
	FklBigInt* bi=NULL;
	if(FKL_IS_F64(obj))
		bi=fklCreateBigIntD(obj->u.f64);
	else if(FKL_IS_BIG_INT(obj))
		bi=fklCopyBigInt(obj->u.bigInt);
	else
		bi=fklCreateBigInt(fklGetInt(obj));
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_i64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i64",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i64",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->i64",frame,exe);
	int64_t r=0;
	if(FKL_IS_F64(obj))
		r=obj->u.f64;
	else
		r=fklGetInt(obj);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_I64,&r,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nth",frame,exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INVALIDACCESS,frame,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiEnd(&ap,stack);
}

void builtin_set_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.set-nth!",frame,exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INVALIDACCESS,frame,exe);
	size_t index=fklGetUint(place);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INVALIDASSIGN,frame,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiEnd(&ap,stack);
}

void builtin_sref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!str)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	size_t index=fklGetUint(place);
	size_t size=str->u.str->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,frame,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,frame,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(str->u.str->str[index]),&ap,stack);
	fklNiEnd(&ap,stack);
}

#define BV_U_S_8_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,frame,exe);\
	r=bvec->u.bvec->ptr[index];\
	fklNiReturn(fklMakeVMint(r,exe),&ap,stack);\
	fklNiEnd(&ap,stack);

#define BV_LT_U64_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,frame,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
		((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];\
	fklNiReturn(fklMakeVMint(r,exe),&ap,stack);\
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!bvec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	size_t index=fklGetUint(place);
	size_t size=bvec->u.bvec->size;
	uint64_t r=0;
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_INVALIDACCESS,frame,exe);
	for(size_t i=0;i<sizeof(r);i++)
		((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];
	if(r>=INT64_MAX)
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCreateBigIntU(r),exe),&ap,stack);
	else
		fklNiReturn(fklMakeVMint(r,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}
#undef BV_LT_U64_REF
#undef BV_U_S_8_REF

#define BV_F_REF(TYPE,WHO) {\
	FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,frame,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];\
	FklVMvalue* f=fklCreateVMvalueToStack(FKL_TYPE_F64,NULL,exe);\
	f->u.f64=r;\
	fklNiReturn(f,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void builtin_bvf32ref(ARGL) BV_F_REF(float,"builtin.bvf32ref")
void builtin_bvf64ref(ARGL) BV_F_REF(double,"builtin.bvf32ref")
#undef BV_F_REF

#define SET_BV_LE_U8_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,frame,exe);\
	bvec->u.bvec->ptr[index]=r;\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);

#define SET_BV_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,frame,exe);\
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
	FklVMframe* frame=exe->frames;\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!FKL_IS_F64(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=target->u.f64;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,frame,exe);\
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!str||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	size_t index=fklGetUint(place);
	size_t size=str->u.str->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INVALIDACCESS,frame,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INVALIDACCESS,frame,exe);
	if(!FKL_IS_CHR(target)&&!fklIsInt(target))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	str->u.str->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_string(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!str||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_CHR(content)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	memset(str->u.str->str,FKL_GET_CHR(content),str->u.str->size);
	fklNiReturn(str,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!bvec||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(content)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	memset(bvec->u.bvec->ptr,fklGetInt(content),bvec->u.bvec->size);
	fklNiReturn(bvec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!vector)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,frame,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,frame,exe);
	fklNiReturn(vector->u.vec->base[index],&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!vector||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INVALIDACCESS,frame,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INVALIDACCESS,frame,exe);
	fklSetRef(&vector->u.vec->base[index],target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-vector!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!vec||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-vector!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.fill-vector!",frame,exe);
	for(size_t i=0;i<vec->u.vec->size;i++)
		fklSetRef(&vec->u.vec->base[i],content,exe->gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cas_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* create_=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!vector||!old||!create_)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INVALIDACCESS,frame,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INVALIDACCESS,frame,exe);
	if(vector->u.vec->base[index]==old)
	{
		fklSetRef(&vector->u.vec->base[index],create_,exe->gc);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_nthcdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nthcdr",frame,exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_INVALIDACCESS,frame,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiEnd(&ap,stack);
}

void builtin_set_nthcdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.set-nthcdr!",frame,exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INVALIDACCESS,frame,exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INVALIDACCESS,frame,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiEnd(&ap,stack);
}

void builtin_length(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_TOOFEWARG,frame,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(fklMakeVMuint(len,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fopen(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* filename=fklNiGetArg(&ap,stack);
	FklVMvalue* mode=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_TOOMANYARG,frame,exe);
	if(!mode||!filename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	char c_filename[filename->u.str->size+1];
	char c_mode[mode->u.str->size+1];
	fklWriteStringToCstr(c_filename,filename->u.str);
	fklWriteStringToCstr(c_mode,mode->u.str);
	FILE* file=fopen(c_filename,c_mode);
	FklVMvalue* obj=NULL;
	if(!file)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.fopen",c_filename,0,FKL_ERR_FILEFAILURE,exe);
	else
		obj=fklCreateVMvalueToStack(FKL_TYPE_FP,fklCreateVMfp(file),exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fclose(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,frame,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.fclose",frame,exe);
	if(fp->u.fp==NULL||fklDestroyVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_INVALIDACCESS,frame,exe);
	fp->u.fp=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_read(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_TOOMANYARG,frame,exe);
	if(stream&&!FKL_IS_FP(stream)&&!FKL_IS_STR(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	char* tmpString=NULL;
	FklVMfp* tmpFile=NULL;
	FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
	if(!stream||FKL_IS_FP(stream))
	{
		tmpFile=stream?stream->u.fp:BuiltInStdin->u.fp;
		pthread_mutex_lock(&tmpFile->lock);
		int unexpectEOF=0;
		size_t size=0;
		tmpString=fklReadInStringPattern(tmpFile->fp,(char**)&tmpFile->prev,&size,&tmpFile->size,0,&unexpectEOF,tokenStack,NULL);
		pthread_mutex_unlock(&tmpFile->lock);
		if(unexpectEOF)
		{
			free(tmpString);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_UNEXPECTEOF,frame,exe);
		}
	}
	else
	{
		const char* parts[]={stream->u.str->str};
		size_t sizes[]={stream->u.str->size};
		uint32_t line=0;
		FklPtrStack* matchStateStack=fklCreatePtrStack(32,16);
		fklSplitStringPartsIntoToken(parts,sizes,1,&line,tokenStack,matchStateStack,NULL,NULL);
		while(!fklIsPtrStackEmpty(matchStateStack))
			free(fklPopPtrStack(matchStateStack));
		fklDestroyPtrStack(matchStateStack);
	}
	size_t errorLine=0;
	FklNastNode* node=fklCreateNastNodeFromTokenStack(tokenStack,&errorLine,builtInHeadSymbolTable);
	FklVMvalue* tmp=NULL;
	if(node==NULL)
		tmp=FKL_VM_NIL;
	else
		tmp=fklCreateVMvalueFromNastNodeAndStoreInStack(node,NULL,exe);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklDestroyToken(fklPopPtrStack(tokenStack));
	fklDestroyPtrStack(tokenStack);
	fklNiReturn(tmp,&ap,stack);
	free(tmpString);
	fklDestroyNastNode(node);
	fklNiEnd(&ap,stack);
}

void builtin_fgets(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOMANYARG,frame,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVMfp* fp=file->u.fp;
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,frame,exe);
	size_t size=fklGetUint(psize);
	char* str=(char*)malloc(sizeof(char)*size);
	FKL_ASSERT(str);
	int32_t realRead=0;
	pthread_mutex_lock(&fp->lock);
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
	pthread_mutex_unlock(&fp->lock);
	if(!realRead)
	{
		free(str);
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
	{
		size_t size=fklGetUint(psize);
		FklVMvalue* vmstr=fklCreateVMvalueToStack(FKL_TYPE_STR,NULL,exe);
		vmstr->u.str=(FklString*)malloc(sizeof(FklString)+size);
		FKL_ASSERT(vmstr->u.str);
		vmstr->u.str->size=size;
		memcpy(vmstr->u.str->str,str,size);
		free(str);
		fklNiReturn(vmstr,&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_fgetb(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOMANYARG,frame,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVMfp* fp=file->u.fp;
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,frame,exe);
	size_t size=fklGetUint(psize);
	uint8_t* ptr=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(ptr);
	int32_t realRead=0;
	pthread_mutex_lock(&fp->lock);
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
	pthread_mutex_unlock(&fp->lock);
	if(!realRead)
	{
		free(ptr);
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
	{
		FklVMvalue* bvec=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(realRead,ptr),exe);
		free(ptr);
		fklNiReturn(bvec,&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_prin1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_TOOFEWARG,frame,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrin1VMvalue(obj,objFile);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_princ(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_TOOFEWARG,frame,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrincVMvalue(obj,objFile);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_dlopen(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* dllName=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlopen",FKL_ERR_TOOMANYARG,frame,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlopen",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(dllName,FKL_IS_STR,"builtin.dlopen",frame,exe);
	char str[dllName->u.str->size+1];
	fklWriteStringToCstr(str,dllName->u.str);
	FklVMdllHandle* dll=fklCreateVMdll(str);
	if(!dll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.dlopen",str,0,FKL_ERR_LOADDLLFAILD,exe);
	FklVMvalue* rel=fklCreateVMvalueToStack(FKL_TYPE_DLL,dll,exe);
	fklInitVMdll(rel);
	fklNiReturn(rel,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_dlsym(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* dll=fklNiGetArg(&ap,stack);
	FklVMvalue* symbol=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_TOOMANYARG,frame,exe);
	if(!dll||!symbol)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(dll))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	if(!dll->u.dll)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_INVALIDACCESS,frame,exe);
	char str[symbol->u.str->size+1];
	fklWriteStringToCstr(str,symbol->u.str);
	FklVMdllFunc funcAddress=fklGetAddress(str,dll->u.dll);
	if(!funcAddress)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.dlsym",str,0,FKL_ERR_INVALIDSYMBOL,exe);
	FklVMdlproc* dlproc=fklCreateVMdlproc(funcAddress,dll);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_DLPROC,dlproc,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_argv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* retval=NULL;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.argv",FKL_ERR_TOOMANYARG,exe->frames,exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&(*tmp)->u.pair->cdr)
		*tmp=fklCreateVMpairV(fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(strlen(argv[i]),argv[i]),exe),FKL_VM_NIL,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_go(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* threadProc=fklNiGetArg(&ap,stack);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!FKL_IS_CONT(threadProc)&&!fklIsCallableUd(threadProc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVM* threadVM=FKL_IS_PROC(threadProc)?fklCreateThreadVM(threadProc->u.proc,exe->gc,exe,exe->next):fklCreateThreadCallableObjVM(frame,exe->gc,threadProc,exe,exe->next);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	FklVMstack* threadVMstack=threadVM->stack;
	fklNiSetBp(threadVMstack->tp,threadVMstack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklPtrStack* comStack=fklCreatePtrStack(32,16);
	for(;cur;cur=fklNiGetArg(&ap,stack))
		fklPushPtrStack(cur,comStack);
	fklNiResBp(&ap,stack);
	while(!fklIsPtrStackEmpty(comStack))
	{
		FklVMvalue* tmp=fklPopPtrStack(comStack);
		fklPushVMvalue(tmp,threadVMstack);
	}
	fklDestroyPtrStack(comStack);
	FklVMvalue* chan=threadVM->chan;
	int32_t faildCode=0;
	faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMfunc,threadVM);
	if(faildCode)
	{
		fklDeleteCallChain(threadVM);
		fklDestroyVMstack(threadVM->stack);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* maxSize=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_TOOMANYARG,frame,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(maxSize,fklIsInt,"builtin.chanl",frame,exe);
	if(fklVMnumberLt0(maxSize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,frame,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_CHAN,fklCreateVMchanl(fklGetUint(maxSize)),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_chanl_num(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-num",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-num",FKL_ERR_TOOFEWARG,frame,exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=obj->u.chan->messageNum;
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-num",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(fklMakeVMuint(len,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_send(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOMANYARG,frame,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.send",frame,exe);
	FklVMsend* t=fklCreateVMsend(message);
	fklChanlSend(t,ch->u.chan);
	fklNiReturn(message,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_recv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	FklVMvalue* okBox=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.recv",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.recv",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.recv",frame,exe);
	if(okBox)
	{
		FKL_NI_CHECK_TYPE(okBox,FKL_IS_BOX,"builtin.recv",frame,exe);
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
		FklVMrecv* t=fklCreateVMrecv();
		fklChanlRecv(t,ch->u.chan);
		fklNiReturn(t->v,&ap,stack);
		fklDestroyVMrecv(t);
	}
	fklNiEnd(&ap,stack);
}

void builtin_error(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* who=fklNiGetArg(&ap,stack);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_TOOMANYARG,frame,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerror((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:who->u.str,FKL_GET_SYM(type),message->u.str),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_raise(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* err=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.raise",FKL_ERR_TOOMANYARG,frame,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.raise",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(err,FKL_IS_ERR,"builtin.raise",frame,exe);
	fklNiEnd(&ap,stack);
	fklRaiseVMerror(err,exe);
}

void builtin_call_eh(ARGL)
{
}

void builtin_call_cc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/cc",FKL_ERR_TOOMANYARG,frame,exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/cc",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.call/cc",frame,exe);
	pthread_rwlock_rdlock(&exe->rlock);
	FklVMcontinuation* cc=fklCreateVMcontinuation(ap,exe);
	if(!cc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/cc",FKL_ERR_CROSS_C_CALL_CONTINUATION,frame,exe);
	FklVMvalue* vcc=fklCreateVMvalueToStack(FKL_TYPE_CONT,cc,exe);
	pthread_rwlock_unlock(&exe->rlock);
	fklNiSetBp(ap,stack);
	fklNiReturn(vcc,&ap,stack);
	if(proc->type==FKL_TYPE_PROC)
	{
		FklVMproc* tmpProc=proc->u.proc;
		FklVMframe* prevProc=fklHasSameProc(tmpProc->scp,exe->frames);
		if(fklIsTheLastExpress(frame,prevProc,exe)&&prevProc)
			prevProc->mark=1;
		else
		{
			FklVMframe* tmpFrame=fklCreateVMframe(tmpProc,exe->frames);
			tmpFrame->localenv=fklCreateVMvalueToStack(FKL_TYPE_ENV,fklCreateVMenv(tmpProc->prevEnv,exe->gc),exe);
			exe->frames=tmpFrame;
		}
	}
	else
		exe->nextCall=proc;
	fklNiEnd(&ap,stack);
}

void builtin_apply(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.apply",frame,exe);
	FklPtrStack* stack1=fklCreatePtrStack(32,16);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_TOOFEWARG,frame,exe);
	FklPtrStack* stack2=fklCreatePtrStack(32,16);
	if(!FKL_IS_PAIR(lastList)&&lastList!=FKL_VM_NIL)
	{
		fklDestroyPtrStack(stack1);
		fklDestroyPtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=fklGetVMpairCdr(lastList))
		fklPushPtrStack(fklGetVMpairCar(lastList),stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklDestroyPtrStack(stack1);
		fklDestroyPtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	}
	fklNiSetBp(ap,stack);
	while(!fklIsPtrStackEmpty(stack2))
	{
		FklVMvalue* t=fklPopPtrStack(stack2);
		fklNiReturn(t,&ap,stack);
	}
	fklDestroyPtrStack(stack2);
	while(!fklIsPtrStackEmpty(stack1))
	{
		FklVMvalue* t=fklPopPtrStack(stack1);
		fklNiReturn(t,&ap,stack);
	}
	fklDestroyPtrStack(stack1);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			applyNativeProc(exe,proc->u.proc,frame);
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
	size_t ap;
}MapCtx;

#define K_MAP_PATTERN(K_FUNC,CUR_PROCESS,RESULT_PROCESS,NEXT_PROCESS) {MapCtx* mapctx=(MapCtx*)ctx;\
	FklVMgc* gc=exe->gc;\
	size_t len=mapctx->len;\
	size_t argNum=mapctx->num;\
	FklVMstack* stack=exe->stack;\
	FklVMframe* frame=exe->frames;\
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
		fklVMcallInDlproc(mapctx->proc,argNum,mapctx->cars->u.vec->base,frame,exe,(K_FUNC),mapctx,sizeof(MapCtx));\
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
	FklVMframe* frame=exe->frames;\
	FklVMvalue* proc=fklNiGetArg(&ap,stack);\
	FklVMgc* gc=exe->gc;\
	if(!proc)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,frame,exe);\
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,(FUNC_NAME),frame,exe);\
	size_t argNum=ap-stack->bp;\
	if(argNum==0)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,frame,exe);\
	FklVMvalue* argVec=fklCreateVMvecV(ap-stack->bp,NULL,exe);\
	for(size_t i=0;i<argNum;i++)\
	{\
		FklVMvalue* cur=fklNiGetArg(&ap,stack);\
		if(!fklIsList(cur))\
		FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);\
		fklSetRef(&argVec->u.vec->base[i],cur,gc);\
	}\
	fklNiResBp(&ap,stack);\
	size_t len=fklVMlistLength(argVec->u.vec->base[0]);\
	for(size_t i=1;i<argNum;i++)\
	if(fklVMlistLength(argVec->u.vec->base[i])!=len)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_LIST_DIFFER_IN_LENGTH,frame,exe);\
	if(len==0)\
	{\
		fklNiReturn((DEFAULT_VALUE),&ap,stack);\
		fklNiEnd(&ap,stack);\
	}\
	else\
	{\
		FklVMvalue* cars=fklCreateVMvecV(argNum,NULL,exe);\
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
		*(mapctx->cur)=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),exe);,
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!obj||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memq",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memq",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.memq",frame,exe);
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
	size_t ap;
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
				,exe->frames,exe,k_member,memberctx,sizeof(MemberCtx));
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!obj||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.member",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.member",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.member",frame,exe);
	if(proc)
	{
		FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.member",frame,exe);
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
	size_t ap;
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
				,exe->frames,exe,k_memp,mempctx,sizeof(MempCtx));
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!proc||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memp",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memp",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.memp",frame,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.memp",frame,exe);
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
	size_t ap;
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
			*filterctx->cur=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),exe);
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
				,exe->frames,exe,k_filter,filterctx,sizeof(FilterCtx));
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*filterctx->cur=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!proc||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.filter",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.filter",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.filter",frame,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.filter",frame,exe);
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
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvalue** r=fklNiGetTopSlot(stack);
	FklVMvalue** pcur=r;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		*pcur=fklCreateVMpairV(cur,FKL_VM_NIL,exe);
		pcur=&(*pcur)->u.pair->cdr;
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(*r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list8(ARGL)
{
	FKL_NI_BEGIN(exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMvalue** r=fklNiGetTopSlot(stack);
	FklVMvalue** pcur=r;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack);cur;)
	{
		FklVMvalue* next=fklNiGetArg(&ap,stack);
		if(next)
		{
			*pcur=fklCreateVMpairV(cur,FKL_VM_NIL,exe);
			pcur=&(*pcur)->u.pair->cdr;
		}
		else
			*pcur=cur;
		cur=next;
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(*r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_reverse(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_PAIR(obj)&&obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(obj!=FKL_VM_NIL)
	{
		for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=cdr->u.pair->cdr)
			retval=fklCreateVMpairV(cdr->u.pair->car,retval,exe);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_feof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.feof",frame,exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_INVALIDACCESS,frame,exe);
	fklNiReturn(!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	size_t size=ap-stack->bp;
	FklVMvalue* vec=fklCreateVMvecV(size,NULL,exe);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],fklNiGetArg(&ap,stack),exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_getdir(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getdir",FKL_ERR_TOOMANYARG,exe->frames,exe);
	FklLineNumTabNode* node=fklFindLineNumTabNode(frame->cp,exe->lnt);
	if(node->fid)
	{
		char* filename=fklCopyCstr(fklGetMainFileRealPath());
		filename=fklStrCat(filename,FKL_PATH_SEPARATOR_STR);
		filename=fklCstrStringCat(filename,fklGetGlobSymbolWithId(node->fid)->symbol);
		char* rpath=fklRealpath(filename);
		char* dir=fklGetDir(rpath);
		free(filename);
		free(rpath);
		FklString* str=fklCreateString(strlen(dir),dir);
		free(dir);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_STR,str,exe),&ap,stack);
	}
	else
	{
		const char* cwd=fklGetCwd();
		FklString* str=fklCreateString(strlen(cwd),cwd);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_STR,str,exe),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_fgetc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_TOOMANYARG,frame,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVMfp* fp=stream?stream->u.fp:BuiltInStdin->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INVALIDACCESS,frame,exe);
	pthread_mutex_lock(&fp->lock);
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
	pthread_mutex_unlock(&fp->lock);
	fklNiEnd(&ap,stack);
}

void builtin_fgeti(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_TOOMANYARG,frame,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVMfp* fp=stream?stream->u.fp:BuiltInStdin->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INVALIDACCESS,frame,exe);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_TOOFEWARG,frame,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.box",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.box",FKL_ERR_TOOFEWARG,frame,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BOX,obj,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_unbox(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.unbox",FKL_ERR_TOOMANYARG,frame,exe);
	if(!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.unbox",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.unbox",frame,exe);
	fklNiReturn(box->u.box,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-box!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!obj||!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-box!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.set-box!",frame,exe);
	fklSetRef(&box->u.box,obj,exe->gc);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cas_box(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* create=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-box!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!old||!box||!create)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-box!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.cas-box!",frame,exe);
	if(box->u.box==old)
	{
		fklSetRef(&box->u.box,create,exe->gc);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	size_t size=ap-stack->bp;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,NULL),exe);
	FklBytevector* bytevec=r->u.bvec;
	size_t i=0;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur!=NULL
			;cur=fklNiGetArg(&ap,stack),i++)
	{
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.bytevector",frame,exe);
		bytevec->ptr[i]=fklGetInt(cur);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-bytevector",frame,exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_TOOMANYARG,frame,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,frame,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(len,NULL),exe);
	FklBytevector* bytevec=r->u.bvec;
	uint8_t u_8=0;
	if(content)
	{
		FKL_NI_CHECK_TYPE(content,fklIsInt,"builtin.make-bytevector",frame,exe);
		u_8=fklGetInt(content);
	}
	memset(bytevec->ptr,u_8,len);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

#define PREDICATE(condtion,err_infor) FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOMANYARG,frame,exe);\
	if(!val)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,frame,exe);\
	if(condtion)\
	fklNiReturn(FKL_VM_TRUE,&ap,stack);\
	else\
	fklNiReturn(FKL_VM_NIL,&ap,stack);\
	fklNiEnd(&ap,stack);

#ifndef _WIN32

static int getch()
{
	struct termios oldt,createt;
	int ch;
	tcgetattr(STDIN_FILENO,&oldt);
	createt=oldt;
	createt.c_lflag &=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&createt);
	ch=getchar();
	tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
	return ch;
}
#endif

void builtin_getch(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getch",FKL_ERR_TOOMANYARG,exe->frames,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(getch()),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sleep(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.sleep",frame,exe);
	fklNiReturn(fklMakeVMint((sleep(fklGetInt(second))),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

//void builtin_usleep(ARGL)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMvalue* second=fklNiGetArg(&ap,stack);
//	FklVMframe* frame=exe->frames;
//	if(!second)
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.usleep",FKL_ERR_TOOFEWARG,frame,exe);
//	if(fklNiResBp(&ap,stack))
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.usleep",FKL_ERR_TOOMANYARG,frame,exe);
//	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.usleep",frame,exe);
//#ifdef _WIN32
//		Sleep(fklGetInt(second));
//#else
//		usleep(fklGetInt(second));
//#endif
//	fklNiReturn(second,&ap,stack);
//	fklNiEnd(&ap,stack);
//}

void builtin_srand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* s=fklNiGetArg(&ap,stack);
    if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.srand",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(s,fklIsInt,"builtin.srand",frame,exe);
    srand(fklGetInt(s));
    fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_rand(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue*  lim=fklNiGetArg(&ap,stack);
	FklVMframe* frame=exe->frames;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_TOOMANYARG,frame,exe);
	if(lim&&!fklIsInt(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(FKL_MAKE_VM_I32(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_get_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get-time",FKL_ERR_TOOMANYARG,exe->frames,exe);
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
	FklString* str=fklCreateString(timeLen-1,trueTime);
	FklVMvalue* tmpVMvalue=fklCreateVMvalueToStack(FKL_TYPE_STR,str,exe);
	free(trueTime);
	fklNiReturn(tmpVMvalue,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_remove_file(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklNiGetArg(&ap,stack);
	FklVMframe* frame=exe->frames;
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.remove-file",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.remove-file",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"builtin.remove-file",frame,exe);
	char str[name->u.str->size+1];
	fklWriteStringToCstr(str,name->u.str);
	fklNiReturn(FKL_MAKE_VM_I32(remove(str)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_time(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.time",FKL_ERR_TOOMANYARG,frame,exe);
	fklNiReturn(fklMakeVMint((int64_t)time(NULL),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_get(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* sym=fklNiGetArg(&ap,stack);
	FklVMvalue* defaultValue=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get",FKL_ERR_TOOMANYARG,frame,exe);
	if(!sym)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(sym,FKL_IS_SYM,"builtin.get",frame,exe);
	FklVMvalue* volatile* pV=fklFindVar(FKL_GET_SYM(sym),frame->localenv->u.env);
	if(!pV)
	{
		if(defaultValue)
			fklNiReturn(defaultValue,&ap,stack);
		else
		{
			char* cstr=fklStringToCstr(fklGetGlobSymbolWithId(FKL_GET_SYM(sym))->symbol);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.get",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		}
	}
	else
		fklNiReturn(*pV,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* sym=fklNiGetArg(&ap,stack);
	FklVMvalue* value=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!sym||!value)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(sym,FKL_IS_SYM,"builtin.set!",frame,exe);
	fklSetRef(fklFindOrAddVar(FKL_GET_SYM(sym),frame->localenv->u.env)
			,value
			,exe->gc);
	fklNiReturn(value,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_system(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklNiGetArg(&ap,stack);
	FklVMframe* frame=exe->frames;
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.system",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.system",FKL_ERR_TOOMANYARG,frame,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"builtin.system",frame,exe);
	char str[name->u.str->size+1];
	fklWriteStringToCstr(str,name->u.str);
	fklNiReturn(fklMakeVMint(system(str),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_hash(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQ);
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		if(!FKL_IS_PAIR(cur))
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		}
		fklSetVMhashTable(cur->u.pair->car,cur->u.pair->cdr,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hash_num(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-num",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-num",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-num",frame,exe);
	fklNiReturn(fklMakeVMuint(ht->u.hash->ht->num,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_hash(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQ);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-hash",FKL_ERR_TOOFEWARG,frame,exe);
		}
		fklSetVMhashTable(key,value,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hasheqv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQV);
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		if(!FKL_IS_PAIR(cur))
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hasheqv",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		}
		fklSetVMhashTable(cur->u.pair->car,cur->u.pair->cdr,ht,exe->gc);
	}
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_make_hasheqv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQV);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-hasheqv",FKL_ERR_TOOFEWARG,frame,exe);
		}
		fklSetVMhashTable(key,value,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hashequal(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQUAL);
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		if(!FKL_IS_PAIR(cur))
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hashequal",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		}
		fklSetVMhashTable(cur->u.pair->car,cur->u.pair->cdr,ht,exe->gc);
	}
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_make_hashequal(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQUAL);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-hashequal",FKL_ERR_TOOFEWARG,frame,exe);
		}
		fklSetVMhashTable(key,value,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_href(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	FklVMvalue* key=fklNiGetArg(&ap,stack);
	FklVMvalue* defa=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht||!key)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.href",frame,exe);
	int ok=0;
	FklVMvalue* retval=fklGetVMhashTable(key,ht->u.hash,&ok);
	if(ok)
		fklNiReturn(retval,&ap,stack);
	else
	{
		if(defa)
			fklNiReturn(defa,&ap,stack);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href",FKL_ERR_NO_VALUE_FOR_KEY,frame,exe);
	}
	fklNiEnd(&ap,stack);
}

void builtin_href1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	FklVMvalue* key=fklNiGetArg(&ap,stack);
	FklVMvalue* toSet=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht||!key||!toSet)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.href!",frame,exe);
	FklVMhashTableItem* item=fklRefVMhashTable1(key,toSet,ht->u.hash,exe->gc);
	fklNiReturn(item->v,&ap,stack);
	fklNiEnd(&ap,stack);
}

//void builtin_hash_clear(ARGL)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMframe* frame=exe->frames;
//	FklVMvalue* ht=fklNiGetArg(&ap,stack);
//	if(fklNiResBp(&ap,stack))
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-clear!",FKL_ERR_TOOMANYARG,frame,exe);
//	if(!ht)
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-clear!",FKL_ERR_TOOFEWARG,frame,exe);
//	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-clear!",frame,exe);
//	fklClearVMhashTable(ht->u.hash,exe->gc);
//	fklNiReturn(ht,&ap,stack);
//	fklNiEnd(&ap,stack);
//}

void builtin_set_href(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	FklVMvalue* key=fklNiGetArg(&ap,stack);
	FklVMvalue* value=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht||!key||!value)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.set-href!",frame,exe);
	fklSetVMhashTable(key,value,ht->u.hash,exe->gc);
	fklNiReturn(value,&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_set_href8(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href*!",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.set-href*!",frame,exe);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href*!",FKL_ERR_TOOFEWARG,frame,exe);
		fklSetVMhashTable(key,value,ht->u.hash,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(ht,&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hash_to_list(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash->list",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash->list",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash->list",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMhashTable* hash=ht->u.hash;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(FklHashTableNodeList* list=hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		FklVMvalue* pair=fklCreateVMpairV(item->key,item->v,exe);
		fklSetRef(cur,fklCreateVMpairV(pair,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_hash_keys(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-keys",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-keys",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-keys",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMhashTable* hash=ht->u.hash;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(FklHashTableNodeList* list=hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklSetRef(cur,fklCreateVMpairV(item->key,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_hash_values(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-values",FKL_ERR_TOOMANYARG,frame,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-values",FKL_ERR_TOOFEWARG,frame,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-values",frame,exe);
	fklPushVMvalue(FKL_VM_NIL,stack);
	FklVMhashTable* hash=ht->u.hash;
	FklVMvalue** pr=fklNiGetTopSlot(stack);
	FklVMvalue** cur=pr;
	for(FklHashTableNodeList* list=hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklSetRef(cur,fklCreateVMpairV(item->v,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(*pr,&ap,stack);
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
void builtin_continuation_p(ARGL) {PREDICATE(FKL_IS_CONT(val),"builtin.continuation?")}
void builtin_vector_p(ARGL) {PREDICATE(FKL_IS_VECTOR(val),"builtin.vector?")}
void builtin_bytevector_p(ARGL) {PREDICATE(FKL_IS_BYTEVECTOR(val),"builtin.bytevector?")}
void builtin_chanl_p(ARGL) {PREDICATE(FKL_IS_CHAN(val),"builtin.chanl?")}
void builtin_dll_p(ARGL) {PREDICATE(FKL_IS_DLL(val),"builtin.dll?")}
void builtin_big_int_p(ARGL) {PREDICATE(FKL_IS_BIG_INT(val),"builtin.big-int?")}
void builtin_list_p(ARGL){PREDICATE(fklIsList(val),"builtin.list?")}
void builtin_box_p(ARGL) {PREDICATE(FKL_IS_BOX(val),"builtin.box?")}
void builtin_hash_p(ARGL) {PREDICATE(FKL_IS_HASHTABLE(val),"builtin.hash?")}
void builtin_hasheq_p(ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQ(val),"builtin.hash?")}
void builtin_hasheqv_p(ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQV(val),"builtin.hash?")}
void builtin_hashequal_p(ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQUAL(val),"builtin.hash?")}

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
	{"call/eh",               builtin_call_eh,                 },
	{"continuation?",         builtin_continuation_p,          },
	{"fopen",                 builtin_fopen,                   },
	{"read",                  builtin_read,                    },
	{"prin1",                 builtin_prin1,                   },
	{"princ",                 builtin_princ,                   },
	{"dlopen",                builtin_dlopen,                  },
	{"dlsym",                 builtin_dlsym,                   },
	{"argv",                  builtin_argv,                    },
	{"go",                    builtin_go,                      },
	{"chanl",                 builtin_chanl,                   },
	{"chanl-num",             builtin_chanl_num,               },
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
	{"big-int?",              builtin_big_int_p,               },
	{"f64?",                  builtin_f64_p,                   },
	{"pair?",                 builtin_pair_p,                  },

	{"symbol?",               builtin_symbol_p,                },
	{"string->symbol",        builtin_string_to_symbol,        },

	{"string?",               builtin_string_p,                },
	{"string",                builtin_string,                  },
	{"substring",             builtin_substring,               },
	{"sub-string",             builtin_sub_string,               },
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
	{"subvector",             builtin_subvector,               },
	{"sub-vector",             builtin_sub_vector,               },
	{"list->vector",          builtin_list_to_vector,          },
	{"string->vector",        builtin_string_to_vector,        },
	{"vref",                  builtin_vref,                    },
	{"set-vref!",             builtin_set_vref,                },
	{"cas-vref!",             builtin_cas_vref,                },
	{"fill-vector!",          builtin_fill_vector,             },

	{"list?",                 builtin_list_p,                  },
	{"list",                  builtin_list,                    },
	{"list*",				  builtin_list8,                   },
	{"make-list",             builtin_make_list,               },
	{"vector->list",          builtin_vector_to_list,          },
	{"string->list",          builtin_string_to_list,          },
	{"set-nth!",              builtin_set_nth,                 },
	{"set-nthcdr!",           builtin_set_nthcdr,              },

	{"bytevector?",           builtin_bytevector_p,            },
	{"bytevector",            builtin_bytevector,              },
	{"subbytevector",         builtin_subbytevector,           },
	{"sub-bytevector",         builtin_sub_bytevector,           },
	{"make-bytevector",       builtin_make_bytevector,         },
	{"string->bytevector",    builtin_string_to_bytevector,    },
	{"vector->bytevector",    builtin_vector_to_bytevector,    },
	{"list->bytevector",      builtin_list_to_bytevector,      },
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
	{"get",                   builtin_get,                     },
	{"getch",                 builtin_getch,                   },
	{"sleep",                 builtin_sleep,                   },
	{"srand",                 builtin_srand,                   },
	{"rand",                  builtin_rand,                    },
	{"get-time",              builtin_get_time,                },
	{"remove-file",           builtin_remove_file,             },
	{"time",                  builtin_time,                    },
	{"system",                builtin_system,                  },

	{"hash",                  builtin_hash,                    },
	{"hash-num",              builtin_hash_num,                },
	{"make-hash",             builtin_make_hash,               },
	{"hasheqv",               builtin_hasheqv,                 },
	{"make-hasheqv",          builtin_make_hasheqv,            },
	{"hashequal",             builtin_hashequal,               },
	{"make-hashequal",        builtin_make_hashequal,          },
	{"hash?",                 builtin_hash_p,                  },
	{"hasheq?",               builtin_hasheq_p,                },
	{"hasheqv?",              builtin_hasheqv_p,               },
	{"hashequal?",            builtin_hashequal_p,             },
	{"href",                  builtin_href,                    },
	{"href!",                 builtin_href1,                   },
	{"set-href!",             builtin_set_href,                },
	{"set-href*!",            builtin_set_href8,               },
	{"hash->list",            builtin_hash_to_list,            },
	{"hash-keys",             builtin_hash_keys,               },
	{"hash-values",           builtin_hash_values,             },

	{NULL,                    NULL,                            },
};

void fklInitCompEnv(FklCompEnv* curEnv)
{
//	for(const struct SymbolFuncStruct* list=builtInSymbolList
//			;list->s!=NULL
//			;list++)
//		fklAddCompDefCstr(list->s,curEnv);
}

void fklInitGlobCodegenEnvWithSymbolTable(FklCodegenEnv* curEnv,FklSymbolTable* symbolTable)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddCodegenDefBySid(fklAddSymbolCstr(list->s,symbolTable)->id,curEnv);
}

void fklInitGlobCodegenEnv(FklCodegenEnv* curEnv)
{
	fklInitGlobCodegenEnvWithSymbolTable(curEnv,fklGetGlobSymbolTable());
}

void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* table)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddSymbolCstr(list->s,table);
}

void fklInitGlobEnv(FklVMenv* obj,FklVMgc* gc)
{
	static const char* builtInHeadSymbolTableCstr[4]=
	{
		"quote",
		"qsquote",
		"unquote",
		"unqtesp",
	};
	for(int i=0;i<4;i++)
		builtInHeadSymbolTable[i]=fklAddSymbolToGlobCstr(builtInHeadSymbolTableCstr[i])->id;
	const struct SymbolFuncStruct* list=builtInSymbolList;
	BuiltInStdin=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stdin),gc);
	BuiltInStdout=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stdout),gc);
	BuiltInStderr=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stderr),gc);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,FKL_VM_NIL,obj);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,BuiltInStdin,obj);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,BuiltInStdout,obj);
	fklFindOrAddVarWithValue(fklAddSymbolToGlobCstr((list++)->s)->id,BuiltInStderr,obj);
	for(;list->s!=NULL;list++)
	{
		FklVMdlproc* proc=fklCreateVMdlproc(list->f,NULL);
		FklSymTabNode* node=fklAddSymbolToGlobCstr(list->s);
		proc->sid=node->id;
		fklFindOrAddVarWithValue(node->id,fklCreateVMvalueNoGC(FKL_TYPE_DLPROC,proc,gc),obj);
	}
}
