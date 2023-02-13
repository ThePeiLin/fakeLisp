#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/base.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/pattern.h>
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

typedef struct
{
	FklVMvalue* sysIn;
	FklVMvalue* sysOut;
	FklVMvalue* sysErr;
	FklStringMatchPattern* patterns;
	FklSid_t builtInHeadSymbolTable[4];
}PublicBuiltInUserData;

static PublicBuiltInUserData* createPublicBuiltInUserData(FklVMvalue* sysIn
		,FklVMvalue* sysOut
		,FklVMvalue* sysErr
		,FklSymbolTable* publicSymbolTable
		)
{
	PublicBuiltInUserData* r=(PublicBuiltInUserData*)malloc(sizeof(PublicBuiltInUserData));
	FKL_ASSERT(r);
	r->patterns=fklInitBuiltInStringPattern(publicSymbolTable);
	r->sysIn=sysIn;
	r->sysOut=sysOut;
	r->sysErr=sysErr;
	return r;
}

static void _public_builtin_userdata_finalizer(void* p)
{
	PublicBuiltInUserData* d=p;
	fklDestroyAllStringPattern(d->patterns);
	free(d);
}

static void _public_builtin_userdata_atomic(void* p,FklVMgc* gc)
{
	PublicBuiltInUserData* d=p;
	fklGC_toGrey(d->sysIn,gc);
	fklGC_toGrey(d->sysOut,gc);
	fklGC_toGrey(d->sysErr,gc);
}

static FklVMudMethodTable PublicBuiltInUserDataMethodTable=
{
	.__princ=NULL,
	.__prin1=NULL,
	.__finalizer=_public_builtin_userdata_finalizer,
	.__equal=NULL,
	.__call=NULL,
	.__cmp=NULL,
	.__write=NULL,
	.__atomic=_public_builtin_userdata_atomic,
	.__append=NULL,
	.__copy=NULL,
	.__length=NULL,
	.__hash=NULL,
};


void fklInitBuiltinErrorType(FklSid_t errorTypeId[FKL_BUILTIN_ERR_NUM],FklSymbolTable* table)
{
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
		"cir-ref",
	};

	for(size_t i=0;i<FKL_BUILTIN_ERR_NUM;i++)
		errorTypeId[i]=fklAddSymbolCstr(builtInErrorType[i],table)->id;
}

FklSid_t fklGetBuiltInErrorType(FklBuiltInErrorType type,FklSid_t errorTypeId[FKL_ERR_INCORRECT_TYPE_VALUE])
{
	return errorTypeId[type];
}

//builtin functions

#define K_FUNC_ARGL FklVM* exe,FklCCState s,void* ctx
void builtin_car(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.car",exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.car",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.car",exe);
	fklNiReturn(obj->u.pair->car,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_car(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.set-car!",exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-car!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.set-car!",exe);
	fklSetRef(&obj->u.pair->car,target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cdr(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.cdr",exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cdr",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.cdr",exe);
	fklNiReturn(obj->u.pair->cdr,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_cdr(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.set-cdr!",exe);
	if(!target||!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-cdr!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.set-cdr",exe);
	fklSetRef(&obj->u.pair->cdr,target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cons(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_REST_ARG(&ap,stack,"builtin.cons",exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cons",FKL_ERR_TOOFEWARG,exe);
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

void builtin_copy(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_TOOMANYARG,exe);
	FklVMvalue* retval=fklCopyVMvalue(obj,exe);
	if(!retval)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.copy",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_append(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=fklNiGetArg(&ap,stack);
		if(cur)
			retval=fklCopyVMvalue(retval,exe);
		if(!retval)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_eq(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOMANYARG,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOFEWARG,exe);
	fklNiReturn(fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void builtin_eqv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOMANYARG,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.eq",FKL_ERR_TOOFEWARG,exe);
	fklNiReturn((fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
		fklNiEnd(&ap,stack);
}

void builtin_equal(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.equal",FKL_ERR_TOOMANYARG,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.equal",FKL_ERR_TOOFEWARG,exe);
	fklNiReturn((fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static inline FklBigInt* create_uninit_big_int(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	return t;
}

void builtin_add(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(fklIsFixint(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsI64AddOverflow(r64,c64))
				fklAddBigIntI(&bi,c64);
			else
				r64+=fklGetInt(cur);
		}
		else if(FKL_IS_BIG_INT(cur))
			fklAddBigInt(&bi,cur->u.bigInt);
		else if(FKL_IS_F64(cur))
			rd+=cur->u.f64;
		else
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.+",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=0.0)
	{
		rd+=r64+fklBigIntToDouble(&bi);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		fklUninitBigInt(&bi);
	}
	else if(FKL_IS_0_BIG_INT(&bi))
	{
		fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
		fklUninitBigInt(&bi);
	}
	else
	{
		fklAddBigIntI(&bi,r64);
		if(fklIsGtLtI64BigInt(&bi))
		{
			FklBigInt* r=create_uninit_big_int();
			*r=bi;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
		}
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(&bi),exe),&ap,stack);
			fklUninitBigInt(&bi);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_add_1(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_TOOMANYARG,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.1+",exe);
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

void builtin_sub(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"builtin.-",exe);
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
			FklBigInt bi=FKL_STACK_INIT;
			fklInitBigInt0(&bi);
			fklSetBigInt(&bi,prev->u.bigInt);
			fklMulBigIntI(&bi,-1);
			if(fklIsGtLtI64BigInt(&bi))
			{
				FklBigInt* r=create_uninit_big_int();
				*r=bi;
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
			}
			else
			{
				fklNiReturn(fklMakeVMint(fklBigIntToI64(&bi),exe),&ap,stack);
				fklUninitBigInt(&bi);
			}
		}
	}
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt0(&bi);
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(fklIsFixint(cur))
			{
				int64_t c64=fklGetInt(cur);
				if(fklIsI64AddOverflow(r64,c64))
					fklAddBigIntI(&bi,c64);
				else
					r64+=c64;
			}
			else if(FKL_IS_BIG_INT(cur))
				fklAddBigInt(&bi,cur->u.bigInt);
			else if(FKL_IS_F64(cur))
				rd+=cur->u.f64;
			else
			{
				fklUninitBigInt(&bi);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=fklGetDouble(prev)-rd-r64-fklBigIntToDouble(&bi);
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
			fklUninitBigInt(&bi);
		}
		else if(FKL_IS_0_BIG_INT(&bi)&&!FKL_IS_BIG_INT(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsI64AddOverflow(p64,-r64))
			{
				fklAddBigIntI(&bi,p64);
				fklSubBigIntI(&bi,r64);
				FklBigInt* r=create_uninit_big_int();
				*r=bi;
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
			}
			else
			{
				r64=p64-r64;
				fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
				fklUninitBigInt(&bi);
			}
		}
		else
		{
			fklSubBigInt(&bi,prev->u.bigInt);
			fklMulBigIntI(&bi,-1);
			fklSubBigIntI(&bi,r64);
			FklBigInt* r=create_uninit_big_int();
			*r=bi;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void builtin_abs(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.abs",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.abs",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.abs",exe);
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

void builtin_sub_1(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_TOOMANYARG,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.-1+",exe);
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

void builtin_mul(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(fklIsFixint(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsI64MulOverflow(r64,c64))
				fklMulBigIntI(&bi,c64);
			else
				r64*=c64;
		}
		else if(FKL_IS_BIG_INT(cur))
			fklMulBigInt(&bi,cur->u.bigInt);
		else if(FKL_IS_F64(cur))
			rd*=cur->u.f64;
		else
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.*",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklNiResBp(&ap,stack);
	if(rd!=1.0)
	{
		rd*=r64*fklBigIntToDouble(&bi);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		fklUninitBigInt(&bi);
	}
	else if(FKL_IS_1_BIG_INT(&bi))
	{
		fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);
		fklUninitBigInt(&bi);
	}
	else
	{
		fklMulBigIntI(&bi,r64);
		FklBigInt* r=create_uninit_big_int();
		*r=bi;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_idiv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsInt,"builtin.//",exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(fklIsFixint(prev))
		{
			r64=fklGetInt(prev);
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,exe);
			if(r64==1)
				fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
			else if(r64==-1)
				fklNiReturn(FKL_MAKE_VM_I32(-1),&ap,stack);
			else
				fklNiReturn(FKL_MAKE_VM_I32(0),&ap,stack);
		}
		else
		{
			if(FKL_IS_0_BIG_INT(prev->u.bigInt))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,exe);
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi))
		{
			fklDestroyBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,exe);
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

void builtin_div(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsVMnumber,"builtin./",exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
			rd=1/prev->u.f64;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		}
		else
		{
			if(fklIsFixint(prev))
			{
				r64=fklGetInt(prev);
				if(!r64)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
				if(r64==1)
					fklNiReturn(FKL_MAKE_VM_I32(1),&ap,stack);
				else if(r64==-1)
					fklNiReturn(FKL_MAKE_VM_I32(-1),&ap,stack);
				else
				{
					rd=1.0/r64;
					fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
				}
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
		}
		if(r64==0||FKL_IS_0_BIG_INT(bi)||rd==0.0)
		{
			fklDestroyBigInt(bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
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

void builtin_mod(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_TOOMANYARG,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=fklGetDouble(fir);
		double as=fklGetDouble(sec);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
		double r=fmod(af,as);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else if(fklIsFixint(fir)&&fklIsFixint(sec))
	{
		int64_t si=fklGetInt(sec);
		int64_t r=fklGetInt(fir)%si;
		if(si==0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
			}
			fklModBigInt(rem,sec->u.bigInt);
		}
		else
		{
			int64_t si=fklGetInt(sec);
			if(si==0)
			{
				fklDestroyBigInt(rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
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

void builtin_eqn(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_TOOFEWARG,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)==0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

void builtin_gt(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_TOOFEWARG,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

void builtin_ge(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_TOOFEWARG,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)>=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

void builtin_lt(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_TOOFEWARG,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

void builtin_le(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_TOOFEWARG,exe);
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
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else if(FKL_IS_USERDATA(cur)&&cur->u.ud->t->__cmp)
			{
				int isUnableToBeCmp=0;
				r=cur->u.ud->t->__cmp(prev,cur,&isUnableToBeCmp)<=0;
				if(isUnableToBeCmp)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

void builtin_char_to_integer(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_CHR,"builtin.char->integer",exe);
	fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_integer_to_char(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->char",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->char",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsInt,"builtin.integer->char",exe);
	fklNiReturn(FKL_MAKE_VM_CHR(fklGetInt(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->vector",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->vector",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsList,"builtin.list->vector",exe);
	size_t len=fklVMlistLength(obj);
	FklVMvalue* r=fklCreateVMvecV(len,NULL,exe);
	for(size_t i=0;obj!=FKL_VM_NIL;i++,obj=obj->u.pair->cdr)
		fklSetRef(&r->u.vec->base[i],obj->u.pair->car,exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->vector",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->vector",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->vector",exe);
	size_t len=obj->u.str->size;
	FklVMvalue* r=fklCreateVMvecV(len,NULL,exe);
	for(size_t i=0;i<len;i++)
		fklSetRef(&r->u.vec->base[i],FKL_MAKE_VM_CHR(obj->u.str->str[i]),exe->gc);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-list",exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-list",FKL_ERR_TOOMANYARG,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-list",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	FklVMvalue* t=content?content:FKL_VM_NIL;
	for(size_t i=0;i<len;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
		fklSetRef(&(*cur)->u.pair->car,t,gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->list",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->list",exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklString* str=obj->u.str;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<str->size;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_CHR(str->str[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_s8_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-list",exe);
	size_t size=obj->u.bvec->size;
	int8_t* s8a=(int8_t*)obj->u.bvec->ptr;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_I32(s8a[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_u8_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-list",exe);
	size_t size=obj->u.bvec->size;
	uint8_t* u8a=obj->u.bvec->ptr;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklCreateVMvalue(FKL_TYPE_PAIR,fklCreateVMpair(),exe),gc);
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_I32(u8a[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_s8_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-vector",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->s8-vector",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->s8-vector",exe);
	size_t size=obj->u.bvec->size;
	int8_t* s8a=(int8_t*)obj->u.bvec->ptr;
	FklVMvalue* vec=fklCreateVMvecV(obj->u.bvec->size,NULL,exe);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_I32(s8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_u8_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->u8-vector",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->u8-vector",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,"builtin.bytevector->u8-vector",exe);
	size_t size=obj->u.bvec->size;
	uint8_t* u8a=obj->u.bvec->ptr;
	FklVMvalue* vec=fklCreateVMvecV(obj->u.bvec->size,NULL,exe);
	for(size_t i=0;i<size;i++)
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_I32(u8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->list",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_VECTOR,"builtin.vector->list",exe);
	FklVMvec* vec=obj->u.vec;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<vec->size;i++)
	{
		fklSetRef(cur,fklCreateVMpairV(vec->base[i],FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
		fklPopVMstack(stack);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	size_t size=ap-stack->bp;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
	FklString* str=r->u.str;
	size_t i=0;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur!=NULL
			;cur=fklNiGetArg(&ap,stack),i++)
	{
		FKL_NI_CHECK_TYPE(cur,FKL_IS_CHR,"builtin.string",exe);
		str->str[i]=FKL_GET_CHR(cur);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-string",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-string",exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-string",FKL_ERR_TOOMANYARG,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(len,NULL),exe);
	FklString* str=r->u.str;
	char ch=0;
	if(content)
	{
		FKL_NI_CHECK_TYPE(content,FKL_IS_CHR,"builtin.make-string",exe);
		ch=FKL_GET_CHR(content);
	}
	memset(str->str,ch,len);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-vector",exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_TOOMANYARG,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-vector",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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

void builtin_substring(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.substring",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.substring",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_STR,"builtin.substring",exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.substring",exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.substring",exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.substring",FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,ostr->u.str->str+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vsize=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vsize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_STR,"builtin.sub-string",exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-string",exe);
	FKL_NI_CHECK_TYPE(vsize,fklIsInt,"builtin.sub-string",exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-string",FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,ostr->u.str->str+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_subbytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subbytevector",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subbytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,"builtin.subbytevector",exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.subbytevector",exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.subbytevector",exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subbytevector",FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,ostr->u.bvec->ptr+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ostr=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vsize=fklNiGetArg(&ap,stack);
	if(!ostr||!vstart||!vsize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,"builtin.sub-bytevector",exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-bytevector",exe);
	FKL_NI_CHECK_TYPE(vsize,fklIsInt,"builtin.sub-bytevector",exe);
	size_t size=ostr->u.str->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-bytevector",FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,ostr->u.bvec->ptr+start),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_subvector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ovec=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vend=fklNiGetArg(&ap,stack);
	if(!ovec||!vstart||!vend)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subvector",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subvector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(ovec,FKL_IS_VECTOR,"builtin.subvector",exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.subvector",exe);
	FKL_NI_CHECK_TYPE(vend,fklIsInt,"builtin.subvector",exe);
	size_t size=ovec->u.vec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.subvector",FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvecV(size,ovec->u.vec->base+start,exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sub_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ovec=fklNiGetArg(&ap,stack);
	FklVMvalue* vstart=fklNiGetArg(&ap,stack);
	FklVMvalue* vsize=fklNiGetArg(&ap,stack);
	if(!ovec||!vstart||!vsize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(ovec,FKL_IS_VECTOR,"builtin.sub-vector",exe);
	FKL_NI_CHECK_TYPE(vstart,fklIsInt,"builtin.sub-vector",exe);
	FKL_NI_CHECK_TYPE(vsize,fklIsInt,"builtin.sub-vector",exe);
	size_t size=ovec->u.vec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sub-vector",FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvecV(size,ovec->u.vec->base+start,exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_TOOFEWARG,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(FKL_IS_SYM(obj))
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR
				,fklCopyString(fklGetSymbolWithId(FKL_GET_SYM(obj),exe->symbolTable)->symbol)
				,exe);
	else if(FKL_IS_STR(obj))
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR
				,fklCopyString(obj->u.str)
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
			FKL_NI_CHECK_TYPE(obj->u.vec->base[i],FKL_IS_CHR,"builtin.->string",exe);
			retval->u.str->str[i]=FKL_GET_CHR(obj->u.vec->base[i]);
		}
	}
	else if(fklIsList(obj))
	{
		size_t size=fklVMlistLength(obj);
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
		for(size_t i=0;i<size;i++)
		{
			FKL_NI_CHECK_TYPE(obj->u.pair->car,FKL_IS_CHR,"builtin.->string",exe);
			retval->u.str->str[i]=FKL_GET_CHR(obj->u.pair->car);
			obj=obj->u.pair->cdr;
		}
	}
	else if(FKL_IS_USERDATA(obj)&&obj->u.ud->t->__to_string)
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,obj->u.ud->t->__to_string(obj->u.ud->data),exe);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_symbol_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->string",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->string",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_SYM,"builtin.symbol->string",exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR
			,fklCopyString(fklGetSymbolWithId(FKL_GET_SYM(obj),exe->symbolTable)->symbol)
			,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_symbol(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->symbol",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->symbol",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->symbol",exe);
	fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbol(obj->u.str,exe->symbolTable)->id),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_symbol_to_integer(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->integer",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.symbol->integer",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_SYM,"builtin.symbol->integer",exe);
	FklVMvalue* r=fklMakeVMint(FKL_GET_SYM(obj),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_number(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->number",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->number",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_STR,"builtin.string->number",exe);
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

void builtin_number_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* radix=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->string",exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR,NULL,exe);
	if(fklIsInt(obj))
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_NI_CHECK_TYPE(radix,fklIsInt,"builtin.number->string",exe);
			int64_t t=fklGetInt(radix);
			if(t!=8&&t!=10&&t!=16)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_INVALIDRADIX,exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->string",FKL_ERR_TOOMANYARG,exe);
		size_t size=snprintf(buf,64,"%lf",obj->u.f64);
		retval->u.str=fklCreateString(size,buf);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_integer_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* radix=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->string",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->string",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsInt,"builtin.integer->string",exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR,NULL,exe);
	uint32_t base=10;
	if(radix)
	{
		FKL_NI_CHECK_TYPE(radix,fklIsInt,"builtin.integer->string",exe);
		int64_t t=fklGetInt(radix);
		if(t!=8&&t!=10&&t!=16)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->string",FKL_ERR_INVALIDRADIX,exe);
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
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_f64_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.f64->string",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.f64->string",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_F64,"builtin.f64->string",exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR,NULL,exe);
	char buf[256]={0};
	size_t size=snprintf(buf,64,"%lf",obj->u.f64);
	retval->u.str=fklCreateString(size,buf);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->string",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.vector->string",exe);
	size_t size=vec->u.vec->size;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
	for(size_t i=0;i<size;i++)
	{
		FKL_NI_CHECK_TYPE(vec->u.vec->base[i],FKL_IS_CHR,"builtin.vector->string",exe);
		r->u.str->str[i]=FKL_GET_CHR(vec->u.vec->base[i]);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bytevector->string",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_BYTEVECTOR,"builtin.bytevector->string",exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(vec->u.bvec->size,(char*)vec->u.bvec->ptr),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_string_to_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.string->bytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(str,FKL_IS_STR,"builtin.string->bytevector",exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(str->u.str->size,(uint8_t*)str->u.str->str),exe);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector_to_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vector->bytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.vector->bytevector",exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(vec->u.str->size,NULL),exe);
	uint64_t size=vec->u.vec->size;
	FklVMvalue** base=vec->u.vec->base;
	uint8_t* ptr=r->u.bvec->ptr;
	for(uint64_t i=0;i<size;i++)
	{
		FklVMvalue* cur=base[i];
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.vector->bytevector",exe);
		ptr[i]=fklGetInt(cur);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->bytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.list->bytevector",exe);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(fklVMlistLength(list),NULL),exe);
	uint8_t* ptr=r->u.bvec->ptr;
	for(size_t i=0;list!=FKL_VM_NIL;i++,list=list->u.pair->cdr)
	{
		FklVMvalue* cur=list->u.pair->car;
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.list->bytevector",exe);
		ptr[i]=fklGetInt(cur);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list_to_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.list->string",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.list->string",exe);
	size_t size=fklVMlistLength(list);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,NULL),exe);
	for(size_t i=0;i<size;i++)
	{
		FKL_NI_CHECK_TYPE(list->u.pair->car,FKL_IS_CHR,"builtin.list->string",exe);
		r->u.str->str[i]=FKL_GET_CHR(list->u.pair->car);
		list=list->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_f64(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOFEWARG,exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_F64,NULL,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.integer->f64",exe);
	if(fklIsFixint(obj))
		retval->u.f64=(double)fklGetInt(obj);
	else if(FKL_IS_BIG_INT(obj))
		retval->u.f64=fklBigIntToDouble(obj->u.bigInt);
	else
		retval->u.f64=obj->u.f64;
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_integer(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->integer",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->integer",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->integer",exe);
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

void builtin_number_to_i32(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i32",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i32",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->i32",exe);
	int32_t r=0;
	if(FKL_IS_F64(obj))
		r=obj->u.f64;
	else
		r=fklGetInt(obj);
	fklNiReturn(FKL_MAKE_VM_I32(r),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_number_to_big_int(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->big-int",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->big-int",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->big-int",exe);
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

void builtin_number_to_i64(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i64",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.number->i64",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.number->i64",exe);
	int64_t r=0;
	if(FKL_IS_F64(obj))
		r=obj->u.f64;
	else
		r=fklGetInt(obj);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_I64,&r,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_nth(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_TOOMANYARG,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nth",exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INVALIDACCESS,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

void builtin_set_nth(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_TOOMANYARG,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.set-nth!",exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INVALIDACCESS,exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INVALIDASSIGN,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nth!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

void builtin_sref(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_TOOMANYARG,exe);
	if(!place||!str)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=str->u.str->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(str->u.str->str[index]),&ap,stack);
	fklNiEnd(&ap,stack);
}

#define BV_U_S_8_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,exe);\
	r=bvec->u.bvec->ptr[index];\
	fklNiReturn(fklMakeVMint(r,exe),&ap,stack);\
	fklNiEnd(&ap,stack);

#define BV_LT_U64_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
		((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];\
	fklNiReturn(fklMakeVMint(r,exe),&ap,stack);\
	fklNiEnd(&ap,stack);

void builtin_bvs8ref(FKL_DL_PROC_ARGL) {BV_U_S_8_REF(int8_t,"builtin.bvs8ref")}
void builtin_bvs16ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(int16_t,"builtin.bvs16ref")}
void builtin_bvs32ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(int32_t,"builtin.bvs32ref")}
void builtin_bvs64ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(int64_t,"builtin.bvs64ref")}

void builtin_bvu8ref(FKL_DL_PROC_ARGL) {BV_U_S_8_REF(uint8_t,"builtin.bvu8ref")}
void builtin_bvu16ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(uint16_t,"builtin.bvu16ref")}
void builtin_bvu32ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(uint32_t,"builtin.bvu32ref")}
void builtin_bvu64ref(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_TOOMANYARG,exe);
	if(!place||!bvec)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=bvec->u.bvec->size;
	uint64_t r=0;
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.bvu64ref",FKL_ERR_INVALIDACCESS,exe);
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
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,exe);\
	if(!place||!bvec)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	((uint8_t*)&r)[i]=bvec->u.bvec->ptr[index+i];\
	FklVMvalue* f=fklCreateVMvalueToStack(FKL_TYPE_F64,NULL,exe);\
	f->u.f64=r;\
	fklNiReturn(f,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void builtin_bvf32ref(FKL_DL_PROC_ARGL) BV_F_REF(float,"builtin.bvf32ref")
void builtin_bvf64ref(FKL_DL_PROC_ARGL) BV_F_REF(double,"builtin.bvf32ref")
#undef BV_F_REF

#define SET_BV_LE_U8_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,exe);\
	bvec->u.bvec->ptr[index]=r;\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);

#define SET_BV_REF(TYPE,WHO) FKL_NI_BEGIN(exe);\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	bvec->u.bvec->ptr[index+i]=((uint8_t*)&r)[i];\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);

void builtin_set_bvs8ref(FKL_DL_PROC_ARGL) {SET_BV_LE_U8_REF(int8_t,"builtin.set-bvs8ref!")}
void builtin_set_bvs16ref(FKL_DL_PROC_ARGL) {SET_BV_REF(int16_t,"builtin.set-bvs16ref!")}
void builtin_set_bvs32ref(FKL_DL_PROC_ARGL) {SET_BV_REF(int32_t,"builtin.set-bvs32ref!")}
void builtin_set_bvs64ref(FKL_DL_PROC_ARGL) {SET_BV_REF(int64_t,"builtin.set-bvs64ref!")}

void builtin_set_bvu8ref(FKL_DL_PROC_ARGL) {SET_BV_LE_U8_REF(uint8_t,"builtin.set-bvu8ref!")}
void builtin_set_bvu16ref(FKL_DL_PROC_ARGL) {SET_BV_REF(uint16_t,"builtin.set-bvu16ref!")}
void builtin_set_bvu32ref(FKL_DL_PROC_ARGL) {SET_BV_REF(uint32_t,"builtin.set-bvu32ref!")}
void builtin_set_bvu64ref(FKL_DL_PROC_ARGL) {SET_BV_REF(uint64_t,"builtin.set-bvu64ref!")}
#undef SET_BV_IU_REF

#define SET_BV_F_REF(TYPE,WHO) {\
	FKL_NI_BEGIN(exe);\
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);\
	FklVMvalue* place=fklNiGetArg(&ap,stack);\
	FklVMvalue* target=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOMANYARG,exe);\
	if(!place||!bvec||!target)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_TOOFEWARG,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!FKL_IS_F64(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	size_t size=bvec->u.bvec->size;\
	TYPE r=target->u.f64;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INVALIDACCESS,exe);\
	for(size_t i=0;i<sizeof(r);i++)\
	bvec->u.bvec->ptr[index+i]=((uint8_t*)&r)[i];\
	fklNiReturn(target,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void builtin_set_bvf32ref(FKL_DL_PROC_ARGL) SET_BV_F_REF(float,"builtin.set-bvf32ref!")
void builtin_set_bvf64ref(FKL_DL_PROC_ARGL) SET_BV_F_REF(double,"builtin.set-bvf64ref!")
#undef SET_BV_F_REF

void builtin_set_sref(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_TOOMANYARG,exe);
	if(!place||!str||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=str->u.str->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INVALIDACCESS,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INVALIDACCESS,exe);
	if(!FKL_IS_CHR(target)&&!fklIsInt(target))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-sref!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	str->u.str->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_string(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_TOOMANYARG,exe);
	if(!str||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_CHR(content)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-string!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	memset(str->u.str->str,FKL_GET_CHR(content),str->u.str->size);
	fklNiReturn(str,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* bvec=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_TOOMANYARG,exe);
	if(!bvec||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(content)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-bytevector!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	memset(bvec->u.bvec->ptr,fklGetInt(content),bvec->u.bvec->size);
	fklNiReturn(bvec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vref(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_TOOMANYARG,exe);
	if(!place||!vector)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,exe);
	fklNiReturn(vector->u.vec->base[index],&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_vref(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_TOOMANYARG,exe);
	if(!place||!vector||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INVALIDACCESS,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-vref!",FKL_ERR_INVALIDACCESS,exe);
	fklSetRef(&vector->u.vec->base[index],target,exe->gc);
	fklNiReturn(target,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fill_vector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vec=fklNiGetArg(&ap,stack);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-vector!",FKL_ERR_TOOMANYARG,exe);
	if(!vec||!content)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fill-vector!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(vec,FKL_IS_VECTOR,"builtin.fill-vector!",exe);
	for(size_t i=0;i<vec->u.vec->size;i++)
		fklSetRef(&vec->u.vec->base[i],content,exe->gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cas_vref(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* create_=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_TOOMANYARG,exe);
	if(!place||!vector||!old||!create_)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INVALIDACCESS,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-vref!",FKL_ERR_INVALIDACCESS,exe);
	if(vector->u.vec->base[index]==old)
	{
		fklSetRef(&vector->u.vec->base[index],create_,exe->gc);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_nthcdr(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_TOOMANYARG,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nthcdr",exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_INVALIDACCESS,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nthcdr",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

void builtin_tail(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.tail",FKL_ERR_TOOMANYARG,exe);
	if(!objlist)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.tail",FKL_ERR_TOOFEWARG,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(;FKL_IS_PAIR(objPair)&&objPair->u.pair->cdr!=FKL_VM_NIL;objPair=fklGetVMpairCdr(objPair));
		fklNiReturn(objPair,&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.tail",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

void builtin_set_nthcdr(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FklVMvalue* target=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_TOOMANYARG,exe);
	if(!place||!objlist||!target)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.set-nthcdr!",exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INVALIDACCESS,exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INVALIDACCESS,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-nthcdr!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

void builtin_length(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_TOOFEWARG,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(fklMakeVMuint(len,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fopen(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* filename=fklNiGetArg(&ap,stack);
	FklVMvalue* mode=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_TOOMANYARG,exe);
	if(!mode||!filename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fopen",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

void builtin_fclose(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.fclose",exe);
	if(fp->u.fp==NULL||fklDestroyVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_INVALIDACCESS,exe);
	fp->u.fp=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_read(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_TOOMANYARG,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	char* tmpString=NULL;
	FklVMfp* tmpFile=NULL;
	FklPtrStack tokenStack=FKL_STACK_INIT;
	fklInitPtrStack(&tokenStack,32,16);
	FklStringMatchRouteNode* route=NULL;
	PublicBuiltInUserData* pbd=pd->u.ud->data;
	int unexpectEOF=0;
	if(!stream||FKL_IS_FP(stream))
	{
		size_t line=1;
		tmpFile=stream?stream->u.fp:pbd->sysIn->u.fp;
		size_t size=0;
		FklStringMatchPattern* patterns=pbd->patterns;
		tmpString=fklReadInStringPattern(tmpFile->fp
				,(char**)&tmpFile->prev
				,&size
				,&tmpFile->size
				,line
				,&line
				,&unexpectEOF
				,&tokenStack
				,NULL
				,patterns
				,&route);
		if(unexpectEOF)
		{
			free(tmpString);
			while(!fklIsPtrStackEmpty(&tokenStack))
				fklDestroyToken(fklPopPtrStack(&tokenStack));
			fklUninitPtrStack(&tokenStack);
			fklDestroyStringMatchRoute(route);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_UNEXPECTEOF,exe);
		}
	}
	size_t errorLine=0;
	PublicBuiltInUserData* publicUserData=pd->u.ud->data;
	FklNastNode* node=fklCreateNastNodeFromTokenStackAndMatchRoute(&tokenStack
			,route
			,&errorLine
			,publicUserData->builtInHeadSymbolTable
			,NULL
			,exe->symbolTable);
	FklVMvalue* tmp=NULL;
	if(node==NULL)
	{
		if(unexpectEOF)
		{
			free(tmpString);
			while(!fklIsPtrStackEmpty(&tokenStack))
				fklDestroyToken(fklPopPtrStack(&tokenStack));
			fklUninitPtrStack(&tokenStack);
			fklDestroyStringMatchRoute(route);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_INVALIDEXPR,exe);
		}
		else
			tmp=FKL_VM_NIL;
	}
	else
		tmp=fklCreateVMvalueFromNastNodeAndStoreInStack(node,NULL,exe);
	while(!fklIsPtrStackEmpty(&tokenStack))
		fklDestroyToken(fklPopPtrStack(&tokenStack));
	fklUninitPtrStack(&tokenStack);
	fklDestroyStringMatchRoute(route);
	fklNiReturn(tmp,&ap,stack);
	free(tmpString);
	fklDestroyNastNode(node);
	fklNiEnd(&ap,stack);
}

void builtin_stringify(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* v=fklNiGetArg(&ap,stack);
	if(!v)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.stringify",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.stringify",FKL_ERR_TOOMANYARG,exe);
	FklString* s=fklStringify(v,exe->symbolTable);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR,s,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_parse(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.parse",FKL_ERR_TOOMANYARG,exe);
	if(!stream)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.parse",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_STR(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.parse",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	char* tmpString=NULL;
	FklPtrStack tokenStack=FKL_STACK_INIT;
	fklInitPtrStack(&tokenStack,32,16);
	FklStringMatchSet* matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	size_t line=1;
	size_t j=0;
	PublicBuiltInUserData* pbd=pd->u.ud->data;
	FklStringMatchPattern* patterns=pbd->patterns;
	FklStringMatchRouteNode* route=fklCreateStringMatchRouteNode(NULL,0,0,NULL,NULL,NULL);
	FklStringMatchRouteNode* troute=route;
	fklSplitStringIntoTokenWithPattern(stream->u.str->str
			,stream->u.str->size
			,line
			,&line
			,j
			,&j
			,&tokenStack
			,matchSet
			,patterns
			,route
			,&troute);
	fklDestroyStringMatchSet(matchSet);
	size_t errorLine=0;
	PublicBuiltInUserData* publicUserData=pd->u.ud->data;
	FklNastNode* node=fklCreateNastNodeFromTokenStackAndMatchRoute(&tokenStack
			,route
			,&errorLine
			,publicUserData->builtInHeadSymbolTable
			,NULL
			,exe->symbolTable);
	fklDestroyStringMatchRoute(route);
	FklVMvalue* tmp=NULL;
	if(node==NULL)
	{
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_INVALIDEXPR,exe);
	}
	else
		tmp=fklCreateVMvalueFromNastNodeAndStoreInStack(node,NULL,exe);
	while(!fklIsPtrStackEmpty(&tokenStack))
		fklDestroyToken(fklPopPtrStack(&tokenStack));
	fklUninitPtrStack(&tokenStack);
	fklNiReturn(tmp,&ap,stack);
	free(tmpString);
	fklDestroyNastNode(node);
	fklNiEnd(&ap,stack);
}

void builtin_fgets(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOMANYARG,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMfp* fp=file->u.fp;
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t size=fklGetUint(psize);
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

void builtin_fgetb(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOMANYARG,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMfp* fp=file->u.fp;
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t size=fklGetUint(psize);
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
		FklVMvalue* bvec=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(realRead,ptr),exe);
		free(ptr);
		fklNiReturn(bvec,&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void builtin_prin1(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_TOOFEWARG,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.prin1",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrin1VMvalue(obj,objFile,exe->symbolTable);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_princ(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_TOOFEWARG,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.princ",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrincVMvalue(obj,objFile,exe->symbolTable);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_newline(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.newline",FKL_ERR_TOOMANYARG,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.newline",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fputc('\n',objFile);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_dlopen(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* dllName=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlopen",FKL_ERR_TOOMANYARG,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlopen",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(dllName,FKL_IS_STR,"builtin.dlopen",exe);
	char str[dllName->u.str->size+1];
	fklWriteStringToCstr(str,dllName->u.str);
	FklVMdll* ndll=fklCreateVMdll(str);
	if(!ndll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.dlopen",str,0,FKL_ERR_LOADDLLFAILD,exe);
	FklVMvalue* dll=fklCreateVMvalueToStack(FKL_TYPE_DLL,ndll,exe);
	fklInitVMdll(dll,exe);
	fklNiReturn(dll,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_dlsym(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ndll=fklNiGetArg(&ap,stack);
	FklVMvalue* symbol=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_TOOMANYARG,exe);
	if(!ndll||!symbol)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(ndll))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(!ndll->u.dll)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.dlsym",FKL_ERR_INVALIDACCESS,exe);
	char str[symbol->u.str->size+1];
	fklWriteStringToCstr(str,symbol->u.str);
	FklVMdllFunc funcAddress=fklGetAddress(str,ndll->u.dll->handle);
	if(!funcAddress)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.dlsym",str,0,FKL_ERR_INVALIDSYMBOL,exe);
	FklVMdlproc* dlproc=fklCreateVMdlproc(funcAddress,ndll,ndll->u.dll->pd);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_DLPROC,dlproc,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_argv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* retval=NULL;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.argv",FKL_ERR_TOOMANYARG,exe);
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

static void* threadVMfunc(void* p)
{
	FklVM* exe=(FklVM*)p;
	FklVMchanl* tmpCh=exe->chan->u.chan;
	int64_t state=fklRunVM(exe);
	fklTcMutexAcquire(exe->gc);
	exe->chan=NULL;
	if(!state)
	{
		FklVMvalue* v=fklGetTopValue(exe->stack);
		FklVMvalue* resultBox=fklCreateVMvalueToStack(FKL_TYPE_BOX,v,exe);
		fklChanlSend(fklCreateVMsend(resultBox),tmpCh,exe->gc);
	}
	else
	{
		FklVMvalue* err=fklGetTopValue(exe->stack);
		fklChanlSend(fklCreateVMsend(err),tmpCh,exe->gc);
	}
	fklDestroyVMstack(exe->stack);
	exe->stack=NULL;
	exe->codeObj=NULL;
	if(state!=0)
		fklDeleteCallChain(exe);
	exe->frames=NULL;
	exe->mark=0;
	fklTcMutexRelease(exe->gc);
	return (void*)state;
}

int matchPattern(FklVMvalue* pattern,FklVMvalue* exp,FklVMhashTable* ht,FklVMgc* gc)
{
	if(!FKL_IS_PAIR(exp))
		return 1;
	if(pattern->u.pair->car!=exp->u.pair->car)
		return 1;
	FklPtrStack s0=FKL_STACK_INIT;
	fklInitPtrStack(&s0,32,16);
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	fklPushPtrStack(pattern->u.pair->cdr,&s0);
	fklPushPtrStack(exp->u.pair->cdr,&s1);
	while(!fklIsPtrStackEmpty(&s0)&&!fklIsPtrStackEmpty(&s1))
	{
		FklVMvalue* v0=fklPopPtrStack(&s0);
		FklVMvalue* v1=fklPopPtrStack(&s1);
		if(FKL_IS_SYM(v0))
			fklSetVMhashTable(v0,v1,ht,gc);
		else if(FKL_IS_PAIR(v0)&&FKL_IS_PAIR(v1))
		{
			fklPushPtrStack(v0->u.pair->cdr,&s0);
			fklPushPtrStack(v0->u.pair->car,&s0);
			fklPushPtrStack(v1->u.pair->cdr,&s1);
			fklPushPtrStack(v1->u.pair->car,&s1);
		}
		else if(!fklVMvalueEqual(v0,v1))
		{
			fklUninitPtrStack(&s0);
			fklUninitPtrStack(&s1);
			return 1;
		}
	}
	fklUninitPtrStack(&s0);
	fklUninitPtrStack(&s1);
	return 0;
}

typedef struct
{
	FklSid_t id;
}SidHashItem;

static SidHashItem* createSidHashItem(FklSid_t key)
{
	SidHashItem* r=(SidHashItem*)malloc(sizeof(SidHashItem));
	FKL_ASSERT(r);
	r->id=key;
	return r;
}

static size_t _sid_hashFunc(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

static void _sid_destroyItem(void* item)
{
	free(item);
}

static int _sid_keyEqual(void* pkey0,void* pkey1)
{
	FklSid_t k0=*(FklSid_t*)pkey0;
	FklSid_t k1=*(FklSid_t*)pkey1;
	return k0==k1;
}

static void* _sid_getKey(void* item)
{
	return &((SidHashItem*)item)->id;
}

static FklHashTableMethodTable SidHashMethodTable=
{
	.__hashFunc=_sid_hashFunc,
	.__destroyItem=_sid_destroyItem,
	.__keyEqual=_sid_keyEqual,
	.__getKey=_sid_getKey,
};

static int isValidSyntaxPattern(const FklVMvalue* p)
{
	if(!FKL_IS_PAIR(p))
		return 0;
	FklVMvalue* head=p->u.pair->car;
	if(!FKL_IS_SYM(head))
		return 0;
	const FklVMvalue* body=p->u.pair->cdr;
	FklHashTable* symbolTable=fklCreateHashTable(8,4,2,0.75,1,&SidHashMethodTable);
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack((void*)body,&stack);
	while(!fklIsPtrStackEmpty(&stack))
	{
		const FklVMvalue* c=fklPopPtrStack(&stack);
		if(FKL_IS_PAIR(c))
		{
			fklPushPtrStack(c->u.pair->cdr,&stack);
			fklPushPtrStack(c->u.pair->car,&stack);
		}
		else if(FKL_IS_SYM(c))
		{
			FklSid_t sid=FKL_GET_SYM(c);
			if(fklGetHashItem(&sid,symbolTable))
			{
				fklDestroyHashTable(symbolTable);
				fklUninitPtrStack(&stack);
				return 0;
			}
			fklPutNoRpHashItem(createSidHashItem(sid)
					,symbolTable);
		}
	}
	fklDestroyHashTable(symbolTable);
	fklUninitPtrStack(&stack);
	return 1;
}

void builtin_pattern_match(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* pattern=fklNiGetArg(&ap,stack);
	FklVMvalue* exp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.pattern-match",FKL_ERR_TOOFEWARG,exe);
	if(!isValidSyntaxPattern(pattern))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.pattern-match",FKL_ERR_INVALIDPATTERN,exe);
	FklVMhashTable* hash=fklCreateVMhashTable(FKL_VM_HASH_EQ);
	if(matchPattern(pattern,exp,hash,exe->gc))
	{
		fklDestroyVMhashTable(hash);
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,hash,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_go(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* threadProc=fklNiGetArg(&ap,stack);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!fklIsCallableUd(threadProc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVM* threadVM=fklCreateThreadVM(exe->gc
			,threadProc
			,exe
			,exe->next
			,exe->libNum
			,exe->libs
			,exe->symbolTable
			,exe->builtinErrorTypeId);
	FklVMstack* threadVMstack=threadVM->stack;
	fklNiSetBp(threadVMstack->tp,threadVMstack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklPtrStack comStack=FKL_STACK_INIT;
	fklInitPtrStack(&comStack,32,16);
	for(;cur;cur=fklNiGetArg(&ap,stack))
		fklPushPtrStack(cur,&comStack);
	fklNiResBp(&ap,stack);
	while(!fklIsPtrStackEmpty(&comStack))
	{
		FklVMvalue* tmp=fklPopPtrStack(&comStack);
		fklPushVMvalue(tmp,threadVMstack);
	}
	fklUninitPtrStack(&comStack);
	FklVMvalue* chan=threadVM->chan;
	int32_t faildCode=0;
	faildCode=pthread_create(&threadVM->tid,NULL,threadVMfunc,threadVM);
	if(faildCode)
	{
		fklDeleteCallChain(threadVM);
		fklDestroyVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->codeObj=NULL;
		fklNiReturn(FKL_MAKE_VM_I32(faildCode),&ap,stack);
	}
	else
		fklNiReturn(chan,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_chanl(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* maxSize=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_TOOMANYARG,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(maxSize,fklIsInt,"builtin.chanl",exe);
	if(fklVMnumberLt0(maxSize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_CHAN,fklCreateVMchanl(fklGetUint(maxSize)),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_chanl_num(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-num",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-num",FKL_ERR_TOOFEWARG,exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=obj->u.chan->messageNum;
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chanl-num",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(fklMakeVMuint(len,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_send(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOMANYARG,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.send",exe);
	FklVMsend* t=fklCreateVMsend(message);
	fklChanlSend(t,ch->u.chan,exe->gc);
	fklNiReturn(message,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_recv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	FklVMvalue* okBox=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.recv",FKL_ERR_TOOMANYARG,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.recv",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.recv",exe);
	if(okBox)
	{
		FKL_NI_CHECK_TYPE(okBox,FKL_IS_BOX,"builtin.recv",exe);
		FklVMvalue* r=FKL_VM_NIL;
		int ok=0;
		fklChanlRecvOk(ch->u.chan,&r,&ok);
		okBox->u.box=ok?FKL_VM_TRUE:FKL_VM_NIL;
		fklNiReturn(r,&ap,stack);
	}
	else
	{
		FklVMrecv* t=fklCreateVMrecv();
		fklChanlRecv(t,ch->u.chan,exe->gc);
		fklNiReturn(t->v,&ap,stack);
		fklDestroyVMrecv(t);
	}
	fklNiEnd(&ap,stack);
}

void builtin_error(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* who=fklNiGetArg(&ap,stack);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_TOOMANYARG,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.error",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerror((FKL_IS_SYM(who))?fklGetSymbolWithId(FKL_GET_SYM(who),exe->symbolTable)->symbol:who->u.str
					,FKL_GET_SYM(type)
					,fklCopyString(message->u.str)),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_raise(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* err=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.raise",FKL_ERR_TOOMANYARG,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.raise",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(err,FKL_IS_ERR,"builtin.raise",exe);
	fklNiEnd(&ap,stack);
	fklRaiseVMerror(err,exe);
}

typedef struct
{
	FklVMvalue* proc;
	FklVMvalue** errorSymbolLists;
	FklVMvalue** errorHandlers;
	size_t num;
	size_t bp;
	size_t top;
}EhFrameContext;


static void error_handler_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
	EhFrameContext* c=(EhFrameContext*)data;
	FklVMdlproc* dlproc=c->proc->u.dlproc;
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

static void error_handler_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
	EhFrameContext* c=(EhFrameContext*)data;
	fklGC_toGrey(c->proc,gc);
	size_t num=c->num;
	for(size_t i=0;i<num;i++)
	{
		fklGC_toGrey(c->errorSymbolLists[i],gc);
		fklGC_toGrey(c->errorHandlers[i],gc);
	}
}

static void error_handler_frame_finalizer(FklCallObjData data)
{
	EhFrameContext* c=(EhFrameContext*)data;
	free(c->errorSymbolLists);
	free(c->errorHandlers);
}

static void error_handler_frame_copy(void* const s[6],void* d[6],FklVM* exe)
{
	EhFrameContext* sc=(EhFrameContext*)s;
	EhFrameContext* dc=(EhFrameContext*)d;
	FklVMgc* gc=exe->gc;
	fklSetRef(&dc->proc,sc->proc,gc);
	size_t num=sc->num;
	dc->num=num;
	dc->errorSymbolLists=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*num);
	FKL_ASSERT(dc->errorSymbolLists);
	dc->errorHandlers=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*num);
	FKL_ASSERT(dc->errorHandlers);
	for(size_t i=0;i<num;i++)
	{
		fklSetRef(&dc->errorSymbolLists[i],sc->errorSymbolLists[i],gc);
		fklSetRef(&dc->errorHandlers[i],sc->errorHandlers[i],gc);
	}
}

static int error_handler_frame_end(FklCallObjData data)
{
	return 1;
}

static void error_handler_frame_step(FklCallObjData data,FklVM* exe)
{
	return;
}

static const FklVMframeContextMethodTable ErrorHandlerContextMethodTable=
{
	.atomic=error_handler_frame_atomic,
	.finalizer=error_handler_frame_finalizer,
	.copy=error_handler_frame_copy,
	.print_backtrace=error_handler_frame_print_backtrace,
	.end=error_handler_frame_end,
	.step=error_handler_frame_step,
};

static int isShouldBeHandle(const FklVMvalue* symbolList,FklSid_t type)
{
	if(symbolList==FKL_VM_NIL)
		return 1;
	for(;symbolList!=FKL_VM_NIL;symbolList=symbolList->u.pair->cdr)
	{
		FklVMvalue* cur=symbolList->u.pair->car;
		FklSid_t sid=FKL_GET_SYM(cur);
		if(sid==type)
			return 1;
	}
	return 0;
}

static int errorCallBackWithErrorHandler(FklVMframe* f,FklVMvalue* errValue,FklVM* exe,void* a)
{
	EhFrameContext* c=(EhFrameContext*)f->u.o.data;
	size_t num=c->num;
	FklVMvalue** errSymbolLists=c->errorSymbolLists;
	FklVMerror* err=errValue->u.err;
	for(size_t i=0;i<num;i++)
	{
		if(isShouldBeHandle(errSymbolLists[i],err->type))
		{
			FklVMstack* stack=exe->stack;
			stack->bps.top=c->top;
			stack->tp=c->bp;
			fklPushVMvalue(errValue,stack);
			fklNiSetBp(c->bp,stack);
			FklVMframe* topFrame=exe->frames;
			exe->frames=f;
			while(topFrame!=f)
			{
				FklVMframe* cur=topFrame;
				topFrame=topFrame->prev;
				fklDestroyVMframe(cur);
			}
			fklTailCallobj(c->errorHandlers[i],f,exe);
			return 1;
		}
	}
	return 0;
}

void builtin_call_eh(FKL_DL_PROC_ARGL)
{
#define GET_LIST (0)
#define GET_PROC (1)

	FKL_NI_BEGIN(exe);
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/eh",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.call/eh",exe);
	FklPtrStack errSymbolLists=FKL_STACK_INIT;
	FklPtrStack errHandlers=FKL_STACK_INIT;
	fklInitPtrStack(&errSymbolLists,32,16);
	fklInitPtrStack(&errHandlers,32,16);
	int state=GET_LIST;
	for(FklVMvalue* v=fklNiGetArg(&ap,stack)
			;v
			;v=fklNiGetArg(&ap,stack))
	{
		switch(state)
		{
			case GET_LIST:
				if(!fklIsSymbolList(v))
				{
					fklUninitPtrStack(&errSymbolLists);
					fklUninitPtrStack(&errHandlers);
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/eh"
							,FKL_ERR_INCORRECT_TYPE_VALUE
							,exe);
				}
				fklPushPtrStack(v,&errSymbolLists);
				break;
			case GET_PROC:
				if(!fklIsCallable(v))
				{
					fklUninitPtrStack(&errSymbolLists);
					fklUninitPtrStack(&errHandlers);
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/eh"
							,FKL_ERR_INCORRECT_TYPE_VALUE
							,exe);
				}
				fklPushPtrStack(v,&errHandlers);
				break;
		}
		state=!state;
	}
	if(state==GET_PROC)
	{
		fklUninitPtrStack(&errSymbolLists);
		fklUninitPtrStack(&errHandlers);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.call/eh",FKL_ERR_TOOFEWARG,exe);
	}
	fklNiResBp(&ap,stack);
	FklVMframe* curframe=exe->frames;
	if(errSymbolLists.top)
	{
		curframe->errorCallBack=errorCallBackWithErrorHandler;
		curframe->u.o.t=&ErrorHandlerContextMethodTable;
		EhFrameContext* c=(EhFrameContext*)curframe->u.o.data;
		c->num=errSymbolLists.top;
		FklVMvalue** t=(FklVMvalue**)realloc(errSymbolLists.base,errSymbolLists.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorSymbolLists=t;
		t=(FklVMvalue**)realloc(errHandlers.base,errHandlers.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorHandlers=t;
		c->bp=ap;
		c->top=stack->bps.top;
	}
	else
	{
		fklUninitPtrStack(&errSymbolLists);
		fklUninitPtrStack(&errHandlers);
	}
	fklNiSetBp(ap,stack);
	fklCallobj(proc,curframe,exe);
	fklNiEnd(&ap,exe->stack);
#undef GET_PROC
#undef GET_LIST
}

void builtin_apply(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.apply",exe);
	FklPtrStack stack1=FKL_STACK_INIT;
	fklInitPtrStack(&stack1,32,16);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=fklNiGetArg(&ap,stack)))
	{
		if(!fklNiResBp(&ap,stack))
		{
			lastList=value;
			break;
		}
		fklPushPtrStack(value,&stack1);
	}
	if(!lastList)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_TOOFEWARG,exe);
	FklPtrStack stack2=FKL_STACK_INIT;
	fklInitPtrStack(&stack2,32,16);
	if(!FKL_IS_PAIR(lastList)&&lastList!=FKL_VM_NIL)
	{
		fklUninitPtrStack(&stack1);
		fklUninitPtrStack(&stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=fklGetVMpairCdr(lastList))
		fklPushPtrStack(fklGetVMpairCar(lastList),&stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklUninitPtrStack(&stack1);
		fklUninitPtrStack(&stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.apply",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	fklNiSetBp(ap,stack);
	while(!fklIsPtrStackEmpty(&stack2))
	{
		FklVMvalue* t=fklPopPtrStack(&stack2);
		fklNiReturn(t,&ap,stack);
	}
	fklUninitPtrStack(&stack2);
	while(!fklIsPtrStackEmpty(&stack1))
	{
		FklVMvalue* t=fklPopPtrStack(&stack1);
		fklNiReturn(t,&ap,stack);
	}
	fklUninitPtrStack(&stack1);
	fklTailCallobj(proc,frame,exe);
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
	if(mapctx->i<len)\
	{\
		CUR_PROCESS\
		for(size_t i=0;i<argNum;i++)\
		{\
			FklVMvalue* pair=mapctx->vec->u.vec->base[i];\
			fklSetRef(&(mapctx->cars)->u.vec->base[i],pair->u.pair->car,gc);\
			fklSetRef(&mapctx->vec->u.vec->base[i],pair->u.pair->cdr,gc);\
		}\
		return fklVMcallInDlproc(mapctx->proc,argNum,mapctx->cars->u.vec->base,frame,exe,(K_FUNC),mapctx,sizeof(MapCtx));\
	}\
	fklNiPopTp(stack);\
	fklNiReturn(*mapctx->r,&mapctx->ap,stack);\
	fklNiEnd(&mapctx->ap,stack);}

#define MAP_PATTERN(FUNC_NAME,K_FUNC,DEFAULT_VALUE) FKL_NI_BEGIN(exe);\
	FklVMvalue* proc=fklNiGetArg(&ap,stack);\
	FklVMgc* gc=exe->gc;\
	if(!proc)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,exe);\
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,(FUNC_NAME),exe);\
	size_t argNum=ap-stack->bp;\
	if(argNum==0)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,exe);\
	FklVMvalue* argVec=fklCreateVMvecV(ap-stack->bp,NULL,exe);\
	for(size_t i=0;i<argNum;i++)\
	{\
		FklVMvalue* cur=fklNiGetArg(&ap,stack);\
		if(!fklIsList(cur))\
		FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
		fklSetRef(&argVec->u.vec->base[i],cur,gc);\
	}\
	fklNiResBp(&ap,stack);\
	size_t len=fklVMlistLength(argVec->u.vec->base[0]);\
	for(size_t i=1;i<argNum;i++)\
	if(fklVMlistLength(argVec->u.vec->base[i])!=len)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_LIST_DIFFER_IN_LENGTH,exe);\
	if(len==0)\
	{\
		fklNiReturn((DEFAULT_VALUE),&ap,stack);\
		fklNiEnd(&ap,stack);\
	}\
	else\
	{\
		FklVMvalue* cars=fklCreateVMvecV(argNum,NULL,exe);\
		FklVMvalue* resultBox=fklCreateVMvalueToStack(FKL_TYPE_BOX,(DEFAULT_VALUE),exe);\
		MapCtx* mapctx=(MapCtx*)malloc(sizeof(MapCtx));\
		FKL_ASSERT(mapctx);\
		mapctx->proc=proc;\
		mapctx->r=&resultBox->u.box;\
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
void builtin_map(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.map",k_map,FKL_VM_NIL)}

static void k_foreach(K_FUNC_ARGL) {K_MAP_PATTERN(k_foreach,,*(mapctx->r)=result;,)}
void builtin_foreach(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.foreach",k_foreach,FKL_VM_NIL)}

static void k_andmap(K_FUNC_ARGL) {K_MAP_PATTERN(k_andmap,,*(mapctx->r)=result;if(result==FKL_VM_NIL)mapctx->i=len;,)}
void builtin_andmap(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.andmap",k_andmap,FKL_VM_TRUE)}

static void k_ormap(K_FUNC_ARGL) {K_MAP_PATTERN(k_ormap,,*(mapctx->r)=result;if(result!=FKL_VM_NIL)mapctx->i=len;,)}
void builtin_ormap(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.ormap",k_ormap,FKL_VM_NIL)}

#undef K_MAP_PATTERN
#undef MAP_PATTERN

void builtin_memq(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!obj||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memq",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memq",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.memq",exe);
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
	if(memberctx->list!=FKL_VM_NIL)
	{
		FklVMvalue* arglist[2]={memberctx->obj,memberctx->list->u.pair->car};
		return fklVMcallInDlproc(memberctx->proc
				,2,arglist
				,exe->frames,exe,k_member,memberctx,sizeof(MemberCtx));
	}
	fklNiPopTp(stack);
	fklNiReturn(*memberctx->r,&memberctx->ap,stack);
	fklNiEnd(&memberctx->ap,stack);
}

void builtin_member(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!obj||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.member",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.member",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.member",exe);
	if(proc)
	{
		FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.member",exe);
		MemberCtx* memberctx=(MemberCtx*)malloc(sizeof(MemberCtx));
		FKL_ASSERT(memberctx);
		FklVMvalue* resultBox=fklCreateVMvalueToStack(FKL_TYPE_BOX,FKL_VM_NIL,exe);
		memberctx->r=&resultBox->u.box;
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
	if(mempctx->list!=FKL_VM_NIL)
	{
		return fklVMcallInDlproc(mempctx->proc
				,1,&mempctx->list->u.pair->car
				,exe->frames,exe,k_memp,mempctx,sizeof(MempCtx));
	}
	fklNiPopTp(stack);
	fklNiReturn(*mempctx->r,&mempctx->ap,stack);
	fklNiEnd(&mempctx->ap,stack);
}

void builtin_memp(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!proc||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memp",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.memp",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.memp",exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.memp",exe);
	MempCtx* mempctx=(MempCtx*)malloc(sizeof(MempCtx));
	FKL_ASSERT(mempctx);
	FklVMvalue* resultBox=fklCreateVMvalueToStack(FKL_TYPE_BOX,FKL_VM_NIL,exe);
	mempctx->r=&resultBox->u.box;
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
	if(filterctx->list!=FKL_VM_NIL)
	{
		return fklVMcallInDlproc(filterctx->proc
				,1,&filterctx->list->u.pair->car
				,exe->frames,exe,k_filter,filterctx,sizeof(FilterCtx));
	}
	fklNiPopTp(stack);
	fklNiReturn(*filterctx->r,&filterctx->ap,stack);
	fklNiEnd(&filterctx->ap,stack);
}

void builtin_filter(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	if(!proc||!list)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.filter",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.filter",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(proc,fklIsCallable,"builtin.filter",exe);
	FKL_NI_CHECK_TYPE(list,fklIsList,"builtin.filter",exe);
	FilterCtx* filterctx=(FilterCtx*)malloc(sizeof(FilterCtx));
	FKL_ASSERT(filterctx);
	FklVMvalue* resultBox=fklCreateVMvalueToStack(FKL_TYPE_BOX,FKL_VM_NIL,exe);
	filterctx->r=&resultBox->u.box;
	filterctx->cur=filterctx->r;
	filterctx->proc=proc;
	filterctx->list=list;
	filterctx->ap=ap;
	k_filter(exe,FKL_CC_OK,filterctx);
}

void builtin_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pcur=&r;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		fklSetRef(pcur,fklCreateVMpairV(cur,FKL_VM_NIL,exe),exe->gc);
		pcur=&(*pcur)->u.pair->cdr;
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_list8(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pcur=&r;
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
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_reverse(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_PAIR(obj)&&obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.reverse",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(obj!=FKL_VM_NIL)
	{
		for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=cdr->u.pair->cdr)
			retval=fklCreateVMpairV(cdr->u.pair->car,retval,exe);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_feof(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_TOOMANYARG,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.feof",exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.feof",FKL_ERR_INVALIDACCESS,exe);
	fklNiReturn(!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_vector(FKL_DL_PROC_ARGL)
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

void builtin_getcwd(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getcwd",FKL_ERR_TOOMANYARG,exe);
	FklVMvalue* s=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateStringFromCstr(fklGetCwd()),exe);
	fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_chdir(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* dir=fklNiGetArg(&ap,stack);
	if(!dir)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chdir",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.chdir",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(dir,FKL_IS_STR,"builtin.chdir",exe);
	char* cdir=fklStringToCstr(dir->u.str);
	int r=fklChangeWorkPath(cdir);
	if(r)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.chdir",cdir,1,FKL_ERR_FILEFAILURE,exe);
	free(cdir);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_fgetc(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_TOOMANYARG,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	PublicBuiltInUserData* pbd=pd->u.ud->data;
	FklVMfp* fp=stream?stream->u.fp:pbd->sysIn->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INVALIDACCESS,exe);
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

void builtin_fgeti(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_TOOMANYARG,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	PublicBuiltInUserData* pbd=pd->u.ud->data;
	FklVMfp* fp=stream?stream->u.fp:pbd->sysIn->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INVALIDACCESS,exe);
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

void builtin_fwrite(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_TOOFEWARG,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	if(FKL_IS_STR(obj)||FKL_IS_SYM(obj))
		fklPrincVMvalue(obj,objFile,exe->symbolTable);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_box(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.box",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.box",FKL_ERR_TOOFEWARG,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BOX,obj,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_unbox(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.unbox",FKL_ERR_TOOMANYARG,exe);
	if(!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.unbox",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.unbox",exe);
	fklNiReturn(box->u.box,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set_box(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-box!",FKL_ERR_TOOMANYARG,exe);
	if(!obj||!box)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-box!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.set-box!",exe);
	fklSetRef(&box->u.box,obj,exe->gc);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_cas_box(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FklVMvalue* old=fklNiGetArg(&ap,stack);
	FklVMvalue* create=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-box!",FKL_ERR_TOOMANYARG,exe);
	if(!old||!box||!create)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cas-box!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.cas-box!",exe);
	if(box->u.box==old)
	{
		fklSetRef(&box->u.box,create,exe->gc);
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	}
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	size_t size=ap-stack->bp;
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,NULL),exe);
	FklBytevector* bytevec=r->u.bvec;
	size_t i=0;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur!=NULL
			;cur=fklNiGetArg(&ap,stack),i++)
	{
		FKL_NI_CHECK_TYPE(cur,fklIsInt,"builtin.bytevector",exe);
		bytevec->ptr[i]=fklGetInt(cur);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_bytevector(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"builtin.make-bytevector",exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_TOOMANYARG,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-bytevector",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(len,NULL),exe);
	FklBytevector* bytevec=r->u.bvec;
	uint8_t u_8=0;
	if(content)
	{
		FKL_NI_CHECK_TYPE(content,fklIsInt,"builtin.make-bytevector",exe);
		u_8=fklGetInt(content);
	}
	memset(bytevec->ptr,u_8,len);
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

#define PREDICATE(condtion,err_infor) FKL_NI_BEGIN(exe);\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOMANYARG,exe);\
	if(!val)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,exe);\
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

void builtin_getch(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getch",FKL_ERR_TOOMANYARG,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(getch()),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_sleep(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.sleep",exe);
	int64_t sec=fklGetInt(second);
	fklTcMutexRelease(exe->gc);
	unsigned int r=sleep(sec);
	fklTcMutexAcquire(exe->gc);
	fklNiReturn(fklMakeVMuint(r,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

//void builtin_usleep(FKL_DL_PROC_ARGL)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMvalue* second=fklNiGetArg(&ap,stack);
//	FklVMframe* frame=exe->frames;
//	if(!second)
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.usleep",FKL_ERR_TOOFEWARG,exe);
//	if(fklNiResBp(&ap,stack))
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.usleep",FKL_ERR_TOOMANYARG,exe);
//	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.usleep",exe);
//#ifdef _WIN32
//		Sleep(fklGetInt(second));
//#else
//		usleep(fklGetInt(second));
//#endif
//	fklNiReturn(second,&ap,stack);
//	fklNiEnd(&ap,stack);
//}

void builtin_srand(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* s=fklNiGetArg(&ap,stack);
    if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.srand",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(s,fklIsInt,"builtin.srand",exe);
    srand(fklGetInt(s));
    fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_rand(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue*  lim=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_TOOMANYARG,exe);
	if(lim&&!fklIsInt(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(FKL_MAKE_VM_I32(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_get_time(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get-time",FKL_ERR_TOOMANYARG,exe);
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

void builtin_remove_file(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklNiGetArg(&ap,stack);
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.remove-file",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.remove-file",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"builtin.remove-file",exe);
	char str[name->u.str->size+1];
	fklWriteStringToCstr(str,name->u.str);
	fklNiReturn(FKL_MAKE_VM_I32(remove(str)),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_time(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.time",FKL_ERR_TOOMANYARG,exe);
	fklNiReturn(fklMakeVMint((int64_t)time(NULL),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_get(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	for(;frame->type!=FKL_FRAME_COMPOUND;frame=frame->prev);
	FklVMvalue* sym=fklNiGetArg(&ap,stack);
	FklVMvalue* defaultValue=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get",FKL_ERR_TOOMANYARG,exe);
	if(!sym)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(sym,FKL_IS_SYM,"builtin.get",exe);
	FklVMvalue* volatile* pV=fklFindVar(FKL_GET_SYM(sym),frame->u.c.localenv->u.env);
	if(!pV)
	{
		if(defaultValue)
			fklNiReturn(defaultValue,&ap,stack);
		else
		{
			char* cstr=fklStringToCstr(fklGetSymbolWithId(FKL_GET_SYM(sym),exe->symbolTable)->symbol);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.get",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		}
	}
	else
		fklNiReturn(*pV,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_get8(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	for(;frame->type!=FKL_FRAME_COMPOUND;frame=frame->prev);
	FklVMvalue* sym=fklNiGetArg(&ap,stack);
	FklVMvalue* scope=fklNiGetArg(&ap,stack);
	FklVMvalue* defaultValue=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get*",FKL_ERR_TOOMANYARG,exe);
	if(!sym)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get*",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(sym,FKL_IS_SYM,"builtin.get*",exe);
	FKL_NI_CHECK_TYPE(scope,fklIsFixint,"builtin.get*",exe);
	FklVMvalue* env=fklGetCompoundFrameLocalenv(frame);
	FklVMvalue* volatile* pV=NULL;
	FklSid_t idOfVar=FKL_GET_SYM(sym);
	if(scope)
	{
		size_t s=fklGetInt(scope);
		if(s<0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.get*",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		if(s)
		{
			for(size_t i=1;i<s;i++)
				env=env->u.env->prev;
			pV=fklFindVar(idOfVar,env->u.env);
		}
		else
			while(!pV&&env&&env!=FKL_VM_NIL)
			{
				pV=fklFindVar(idOfVar,env->u.env);
				env=env->u.env->prev;
			}
	}
	else
		pV=fklFindVar(idOfVar,env->u.env);
	if(!pV)
	{
		if(defaultValue)
			fklNiReturn(defaultValue,&ap,stack);
		else
		{
			char* cstr=fklStringToCstr(fklGetSymbolWithId(FKL_GET_SYM(sym),exe->symbolTable)->symbol);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.get",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		}
	}
	else
		fklNiReturn(*pV,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_set(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	for(;frame->type!=FKL_FRAME_COMPOUND;frame=frame->prev);
	FklVMvalue* sym=fklNiGetArg(&ap,stack);
	FklVMvalue* value=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set!",FKL_ERR_TOOMANYARG,exe);
	if(!sym||!value)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(sym,FKL_IS_SYM,"builtin.set!",exe);
	FklSid_t sid=FKL_GET_SYM(sym);
	FklVMvalue* volatile* pv=fklFindOrAddVar(sid,frame->u.c.localenv->u.env);
	fklSetRef(pv,value,exe->gc);
	fklNiReturn(value,&ap,stack);
	fklNiEnd(&ap,stack);
	fklNiDoSomeAfterSetq(*pv,sid);
}

void builtin_system(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* name=fklNiGetArg(&ap,stack);
	if(!name)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.system",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.system",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(name,FKL_IS_STR,"builtin.system",exe);
	char str[name->u.str->size+1];
	fklWriteStringToCstr(str,name->u.str);
	fklNiReturn(fklMakeVMint(system(str),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_hash(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQ);
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		if(!FKL_IS_PAIR(cur))
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		fklSetVMhashTable(cur->u.pair->car,cur->u.pair->cdr,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hash_num(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-num",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-num",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-num",exe);
	fklNiReturn(fklMakeVMuint(ht->u.hash->ht->num,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_make_hash(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQ);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-hash",FKL_ERR_TOOFEWARG,exe);
		}
		fklSetVMhashTable(key,value,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hasheqv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQV);
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		if(!FKL_IS_PAIR(cur))
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hasheqv",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		fklSetVMhashTable(cur->u.pair->car,cur->u.pair->cdr,ht,exe->gc);
	}
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_make_hasheqv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQV);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-hasheqv",FKL_ERR_TOOFEWARG,exe);
		}
		fklSetVMhashTable(key,value,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hashequal(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQUAL);
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack)
			;cur
			;cur=fklNiGetArg(&ap,stack))
	{
		if(!FKL_IS_PAIR(cur))
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hashequal",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		fklSetVMhashTable(cur->u.pair->car,cur->u.pair->cdr,ht,exe->gc);
	}
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_make_hashequal(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMhashTable* ht=fklCreateVMhashTable(FKL_VM_HASH_EQUAL);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
		{
			fklDestroyVMhashTable(ht);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.make-hashequal",FKL_ERR_TOOFEWARG,exe);
		}
		fklSetVMhashTable(key,value,ht,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,ht,exe),&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_href(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	FklVMvalue* key=fklNiGetArg(&ap,stack);
	FklVMvalue* defa=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href",FKL_ERR_TOOMANYARG,exe);
	if(!ht||!key)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.href",exe);
	int ok=0;
	FklVMvalue* retval=fklGetVMhashTable(key,ht->u.hash,&ok);
	if(ok)
		fklNiReturn(retval,&ap,stack);
	else
	{
		if(defa)
			fklNiReturn(defa,&ap,stack);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href",FKL_ERR_NO_VALUE_FOR_KEY,exe);
	}
	fklNiEnd(&ap,stack);
}

void builtin_href1(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	FklVMvalue* key=fklNiGetArg(&ap,stack);
	FklVMvalue* toSet=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href!",FKL_ERR_TOOMANYARG,exe);
	if(!ht||!key||!toSet)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.href!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.href!",exe);
	FklVMhashTableItem* item=fklRefVMhashTable1(key,toSet,ht->u.hash,exe->gc);
	fklNiReturn(item->v,&ap,stack);
	fklNiEnd(&ap,stack);
}

//void builtin_hash_clear(FKL_DL_PROC_ARGL)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMframe* frame=exe->frames;
//	FklVMvalue* ht=fklNiGetArg(&ap,stack);
//	if(fklNiResBp(&ap,stack))
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-clear!",FKL_ERR_TOOMANYARG,exe);
//	if(!ht)
//		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-clear!",FKL_ERR_TOOFEWARG,exe);
//	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-clear!",exe);
//	fklClearVMhashTable(ht->u.hash,exe->gc);
//	fklNiReturn(ht,&ap,stack);
//	fklNiEnd(&ap,stack);
//}

void builtin_set_href(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	FklVMvalue* key=fklNiGetArg(&ap,stack);
	FklVMvalue* value=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href!",FKL_ERR_TOOMANYARG,exe);
	if(!ht||!key||!value)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.set-href!",exe);
	fklSetVMhashTable(key,value,ht->u.hash,exe->gc);
	fklNiReturn(value,&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_set_href8(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href*!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.set-href*!",exe);
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href*!",FKL_ERR_TOOFEWARG,exe);
		fklSetVMhashTable(key,value,ht->u.hash,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(ht,&ap,stack);;
	fklNiEnd(&ap,stack);
}

void builtin_hash_to_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash->list",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash->list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash->list",exe);
	FklVMhashTable* hash=ht->u.hash;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableNodeList* list=hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		FklVMvalue* pair=fklCreateVMpairV(item->key,item->v,exe);
		fklSetRef(cur,fklCreateVMpairV(pair,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_hash_keys(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-keys",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-keys",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-keys",exe);
	FklVMhashTable* hash=ht->u.hash;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableNodeList* list=hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklSetRef(cur,fklCreateVMpairV(item->key,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_hash_values(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-values",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-values",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-values",exe);
	FklVMhashTable* hash=ht->u.hash;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableNodeList* list=hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklSetRef(cur,fklCreateVMpairV(item->v,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_not(FKL_DL_PROC_ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.not")}
void builtin_null(FKL_DL_PROC_ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.null")}
void builtin_atom(FKL_DL_PROC_ARGL) {PREDICATE(!FKL_IS_PAIR(val),"builtin.atom?")}
void builtin_char_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_CHR(val),"builtin.char?")}
void builtin_integer_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val)||FKL_IS_BIG_INT(val),"builtin.integer?")}
void builtin_fix_int_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val),"builtin.fix-int?")}
void builtin_i32_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_I32(val),"builtin.i32?")}
void builtin_i64_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_I64(val),"builtin.i64?")}
void builtin_f64_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_F64(val),"builtin.i64?")}
void builtin_number_p(FKL_DL_PROC_ARGL) {PREDICATE(fklIsVMnumber(val),"builtin.number?")}
void builtin_pair_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_PAIR(val),"builtin.pair?")}
void builtin_symbol_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_SYM(val),"builtin.symbol?")}
void builtin_string_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_STR(val),"builtin.string?")}
void builtin_error_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_ERR(val),"builtin.error?")}
void builtin_procedure_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_PROC(val)||FKL_IS_DLPROC(val),"builtin.procedure?")}
void builtin_proc_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_PROC(val),"builtin.proc?")}
void builtin_dlproc_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_DLPROC(val),"builtin.dlproc?")}
void builtin_vector_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_VECTOR(val),"builtin.vector?")}
void builtin_bytevector_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_BYTEVECTOR(val),"builtin.bytevector?")}
void builtin_chanl_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_CHAN(val),"builtin.chanl?")}
void builtin_dll_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_DLL(val),"builtin.dll?")}
void builtin_big_int_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_BIG_INT(val),"builtin.big-int?")}
void builtin_list_p(FKL_DL_PROC_ARGL){PREDICATE(fklIsList(val),"builtin.list?")}
void builtin_box_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_BOX(val),"builtin.box?")}
void builtin_hash_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE(val),"builtin.hash?")}
void builtin_hasheq_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQ(val),"builtin.hash?")}
void builtin_hasheqv_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQV(val),"builtin.hash?")}
void builtin_hashequal_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQUAL(val),"builtin.hash?")}

void builtin_odd_p(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.odd?",FKL_ERR_TOOMANYARG,exe);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.odd?",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(val,fklIsInt,"builtin.odd?",exe);
	int r=0;
	if(fklIsFixint(val))
		r=fklGetInt(val)%2;
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklSetBigInt(&bi,val->u.bigInt);
		fklModBigIntI(&bi,2);
		r=!FKL_IS_0_BIG_INT(&bi);
	}
	if(r)
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void builtin_even_p(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.even?",FKL_ERR_TOOMANYARG,exe);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.even?",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(val,fklIsInt,"builtin.even?",exe);
	int r=0;
	if(fklIsFixint(val))
		r=fklGetInt(val)%2==0;
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklSetBigInt(&bi,val->u.bigInt);
		fklModBigIntI(&bi,2);
		r=FKL_IS_0_BIG_INT(&bi);
	}
	if(r)
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

#undef K_FUNC_ARGL
#undef PREDICATE
//end

static const struct SymbolFuncStruct
{
	const char* s;
	FklVMdllFunc f;
}builtInSymbolList[]=
{
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
	//{"call/cc",               builtin_call_cc,                 },
	{"call/eh",               builtin_call_eh,                 },
	//{"continuation?",         builtin_continuation_p,          },
	{"fopen",                 builtin_fopen,                   },
	{"read",                  builtin_read,                    },
	{"parse",                 builtin_parse,                   },
	{"stringify",             builtin_stringify,               },
	{"prin1",                 builtin_prin1,                   },
	{"princ",                 builtin_princ,                   },
	{"newline",               builtin_newline,                 },
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
	{"tail",                  builtin_tail,                    },
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
	{"sub-string",            builtin_sub_string,              },
	{"make-string",           builtin_make_string,             },
	{"symbol->string",        builtin_symbol_to_string,        },
	{"number->string",        builtin_number_to_string,        },
	{"integer->string",       builtin_integer_to_string,       },
	{"f64->string",           builtin_f64_to_string,           },
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
	{"sub-vector",            builtin_sub_vector,              },
	{"list->vector",          builtin_list_to_vector,          },
	{"string->vector",        builtin_string_to_vector,        },
	{"vref",                  builtin_vref,                    },
	{"set-vref!",             builtin_set_vref,                },
	{"cas-vref!",             builtin_cas_vref,                },
	{"fill-vector!",          builtin_fill_vector,             },

	{"list?",                 builtin_list_p,                  },
	{"list",                  builtin_list,                    },
	{"list*",                 builtin_list8,                   },
	{"make-list",             builtin_make_list,               },
	{"vector->list",          builtin_vector_to_list,          },
	{"string->list",          builtin_string_to_list,          },
	{"set-nth!",              builtin_set_nth,                 },
	{"set-nthcdr!",           builtin_set_nthcdr,              },

	{"bytevector?",           builtin_bytevector_p,            },
	{"bytevector",            builtin_bytevector,              },
	{"subbytevector",         builtin_subbytevector,           },
	{"sub-bytevector",        builtin_sub_bytevector,          },
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
	{"getcwd",                builtin_getcwd,                  },
	{"chdir",                 builtin_chdir,                   },
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
	{"get*",                  builtin_get8,                    },
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

	{"pattern-match",         builtin_pattern_match,           },

	{"odd?",                  builtin_odd_p,                   },
	{"even?",                 builtin_even_p,                  },

	{NULL,                    NULL,                            },
};

//void fklInitCompEnv(FklCompEnv* curEnv)
//{
//	for(const struct SymbolFuncStruct* list=builtInSymbolList
//			;list->s!=NULL
//			;list++)
//		fklAddCompDefCstr(list->s,curEnv);
//}

void fklInitGlobCodegenEnv(FklCodegenEnv* curEnv,FklSymbolTable* publicSymbolTable)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddCodegenDefBySid(fklAddSymbolCstr(list->s,publicSymbolTable)->id,curEnv);
}

void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* table)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddSymbolCstr(list->s,table);
}

void fklInitGlobEnv(FklVMenv* obj,FklVMgc* gc,FklSymbolTable* table)
{
	static const char* builtInHeadSymbolTableCstr[4]=
	{
		"quote",
		"qsquote",
		"unquote",
		"unqtesp",
	};
	const struct SymbolFuncStruct* list=builtInSymbolList;
	FklVMvalue* builtInStdin=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stdin),gc);
	FklVMvalue* builtInStdout=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stdout),gc);
	FklVMvalue* builtInStderr=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stderr),gc);
	PublicBuiltInUserData* pd=createPublicBuiltInUserData(builtInStdin
					,builtInStdout
					,builtInStderr
					,table);
	FklVMvalue* publicUserData=fklCreateVMvalueNoGC(FKL_TYPE_USERDATA
			,fklCreateVMudata(0
				,&PublicBuiltInUserDataMethodTable
				,pd
				,FKL_VM_NIL
				,FKL_VM_NIL)
			,gc);
	for(int i=0;i<3;i++)
		pd->builtInHeadSymbolTable[i]=fklAddSymbolCstr(builtInHeadSymbolTableCstr[i],table)->id;
	fklFindOrAddVarWithValue(fklAddSymbolCstr((list++)->s,table)->id,builtInStdin,obj);
	fklFindOrAddVarWithValue(fklAddSymbolCstr((list++)->s,table)->id,builtInStdout,obj);
	fklFindOrAddVarWithValue(fklAddSymbolCstr((list++)->s,table)->id,builtInStderr,obj);
	for(;list->s!=NULL;list++)
	{
		FklVMdlproc* proc=fklCreateVMdlproc(list->f,NULL,publicUserData);
		FklSymTabNode* node=fklAddSymbolCstr(list->s,table);
		proc->sid=node->id;
		fklFindOrAddVarWithValue(node->id,fklCreateVMvalueNoGC(FKL_TYPE_DLPROC,proc,gc),obj);
	}
}
