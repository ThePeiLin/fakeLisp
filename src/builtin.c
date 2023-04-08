#include "fakeLisp/vm.h"
#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/base.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/builtin.h>
#include<fakeLisp/codegen.h>

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
#include<unistd.h>
#endif

typedef struct
{
	FklVMvalue* sysIn;
	FklVMvalue* sysOut;
	FklVMvalue* sysErr;
	FklStringMatchPattern* patterns;
	FklSid_t builtInHeadSymbolTable[4];
}PublicBuiltInData;

static void _public_builtin_userdata_finalizer(FklVMudata* p)
{
	FKL_DECL_UD_DATA(d,PublicBuiltInData,p);
	fklDestroyAllStringPattern(d->patterns);
}

static void _public_builtin_userdata_atomic(const FklVMudata* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(d,PublicBuiltInData,ud);
	fklGC_toGrey(d->sysIn,gc);
	fklGC_toGrey(d->sysOut,gc);
	fklGC_toGrey(d->sysErr,gc);
}

static FklVMudMethodTable PublicBuiltInDataMethodTable=
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
static void builtin_car(FKL_DL_PROC_ARGL)
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

static void builtin_set_car(FKL_DL_PROC_ARGL)
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

static void builtin_cdr(FKL_DL_PROC_ARGL)
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

static void builtin_set_cdr(FKL_DL_PROC_ARGL)
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

static void builtin_cons(FKL_DL_PROC_ARGL)
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
	return fklAppendVMudata(&retval->u.ud,cur->u.ud);
}

static int (*const valueAppend[FKL_TYPE_CODE_OBJ+1])(FklVMvalue* retval,FklVMvalue* cur)=
{
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
};

static void builtin_copy(FKL_DL_PROC_ARGL)
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

static inline FklVMvalue* get_fast_value(FklVMvalue* head)
{
	return FKL_IS_PAIR(head)
		&&FKL_IS_PAIR(head->u.pair->cdr)
		&&FKL_IS_PAIR(head->u.pair->cdr->u.pair->cdr)?head->u.pair->cdr->u.pair->cdr:FKL_VM_NIL;
}

static inline FklVMvalue* get_initial_fast_value(FklVMvalue* pr)
{
	return FKL_IS_PAIR(pr)?(pr)->u.pair->cdr:FKL_VM_NIL;
}

static inline FklVMvalue** copy_list(FklVMvalue** pv,FklVM* exe)
{
	FklVMvalue* v=*pv;
	for(;FKL_IS_PAIR(v);v=v->u.pair->cdr,pv=&(*pv)->u.pair->cdr)
		*pv=fklCreateVMpairV(v->u.pair->car,v->u.pair->cdr,exe);
	return pv;
}

static void builtin_append(FKL_DL_PROC_ARGL)
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
			FklVMvalue* pr=*prev;
			for(FklVMvalue* head=get_initial_fast_value(pr)
					;FKL_IS_PAIR(pr)
					;pr=pr->u.pair->cdr
					,head=get_fast_value(head))
				if(head==pr)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append!",FKL_ERR_CIR_REF,exe);
			prev=copy_list(prev,exe);
			for(;FKL_IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			if(*prev==FKL_VM_NIL)
				*prev=cur;
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_append1(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=fklNiGetArg(&ap,stack);
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	else
	{
		FklVMvalue** prev=&retval;
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			for(FklVMvalue* head=get_initial_fast_value(*prev)
					;FKL_IS_PAIR(*prev)
					;prev=&(*prev)->u.pair->cdr
					,head=get_fast_value(head))
				if(head==*prev)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append!",FKL_ERR_CIR_REF,exe);
			if(*prev==FKL_VM_NIL)
				*prev=cur;
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.append!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_eq(FKL_DL_PROC_ARGL)
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

static void builtin_eqv(FKL_DL_PROC_ARGL)
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

static void builtin_equal(FKL_DL_PROC_ARGL)
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

static void builtin_add(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(FKL_IS_FIX(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsFixAddOverflow(r64,c64))
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
		if(fklIsGtLtFixBigInt(&bi))
		{
			FklBigInt* r=create_uninit_big_int();
			*r=bi;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
		}
		else
		{
			fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(&bi)),&ap,stack);
			fklUninitBigInt(&bi);
		}
	}
	fklNiEnd(&ap,stack);
}

static void builtin_add_1(FKL_DL_PROC_ARGL)
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
			if(fklIsGeLeFixBigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklCreateBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(arg->u.bigInt)+1),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==FKL_FIX_INT_MAX)
			{
				FklBigInt* bi=fklCreateBigInt(i);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(FKL_MAKE_VM_FIX(i+1),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

static void builtin_sub(FKL_DL_PROC_ARGL)
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
		else if(FKL_IS_FIX(prev))
		{
			int64_t p64=fklGetInt(prev);
			if(fklIsFixMulOverflow(p64,-1))
			{
				FklBigInt* bi=fklCreateBigInt(p64);
				fklMulBigIntI(bi,-1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(-p64,exe),&ap,stack);
		}
		else
		{
			FklBigInt bi=FKL_STACK_INIT;
			fklInitBigInt0(&bi);
			fklSetBigInt(&bi,prev->u.bigInt);
			fklMulBigIntI(&bi,-1);
			if(fklIsGtLtFixBigInt(&bi))
			{
				FklBigInt* r=create_uninit_big_int();
				*r=bi;
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
			}
			else
			{
				fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(&bi)),&ap,stack);
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
			if(FKL_IS_FIX(cur))
			{
				int64_t c64=fklGetInt(cur);
				if(fklIsFixAddOverflow(r64,c64))
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
			if(fklIsFixAddOverflow(p64,-r64))
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

static void builtin_abs(FKL_DL_PROC_ARGL)
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
		if(FKL_IS_FIX(obj))
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

static void builtin_sub_1(FKL_DL_PROC_ARGL)
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
			if(fklIsGeLeFixBigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklCreateBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(arg->u.bigInt)-1),&ap,stack);
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
				fklNiReturn(fklMakeVMint(i-1,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

static void builtin_mul(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(FKL_IS_FIX(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsFixMulOverflow(r64,c64))
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

static void builtin_idiv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(prev,fklIsInt,"builtin.//",exe);
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_TOOFEWARG,exe);
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(FKL_IS_FIX(cur))
		{
			int64_t c64=fklGetInt(cur);
			if(fklIsFixMulOverflow(r64,c64))
				fklMulBigIntI(&bi,c64);
			else
				r64*=c64;
		}
		else if(FKL_IS_BIG_INT(cur))
			fklMulBigInt(&bi,cur->u.bigInt);
		else
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	if(r64==0||FKL_IS_0_BIG_INT(&bi))
	{
		fklUninitBigInt(&bi);
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,exe);
	}
	fklNiResBp(&ap,stack);
	if(FKL_IS_BIG_INT(prev)&&!FKL_IS_1_BIG_INT(&bi))
	{
		FklBigInt* t=fklCreateBigInt0();
		fklSetBigInt(t,prev->u.bigInt);
		fklDivBigInt(t,&bi);
		fklDivBigIntI(t,r64);
		if(fklIsGtLtFixBigInt(t))
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
	fklUninitBigInt(&bi);
	fklNiEnd(&ap,stack);
}

static void builtin_div(FKL_DL_PROC_ARGL)
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
			if(!islessgreater(prev->u.f64,0.0))
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
			rd=1/prev->u.f64;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		}
		else
		{
			if(FKL_IS_FIX(prev))
			{
				r64=fklGetInt(prev);
				if(!r64)
					FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
				if(r64==1)
					fklNiReturn(FKL_MAKE_VM_FIX(1),&ap,stack);
				else if(r64==-1)
					fklNiReturn(FKL_MAKE_VM_FIX(-1),&ap,stack);
				else
				{
					rd=1.0/r64;
					fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
				}
			}
			else
			{
				if(FKL_IS_1_BIG_INT(prev->u.bigInt))
					fklNiReturn(FKL_MAKE_VM_FIX(1),&ap,stack);
				else if(FKL_IS_N_1_BIG_INT(prev->u.bigInt))
					fklNiReturn(FKL_MAKE_VM_FIX(-1),&ap,stack);
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
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt1(&bi);
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(FKL_IS_FIX(cur))
			{
				int64_t c64=fklGetInt(cur);
				if(fklIsFixMulOverflow(r64,c64))
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
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
		}
		if(r64==0
				||FKL_IS_0_BIG_INT(&bi)
				||!islessgreater(rd,0.0))
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)
				||rd!=1.0
				||(FKL_IS_FIX(prev)
					&&FKL_IS_1_BIG_INT(&bi)
					&&fklGetInt(prev)%(r64))
				||(FKL_IS_BIG_INT(prev)
					&&((!FKL_IS_1_BIG_INT(&bi)&&!fklIsDivisibleBigInt(prev->u.bigInt,&bi))
						||!fklIsDivisibleBigIntI(prev->u.bigInt,r64))))
		{
			rd=fklGetDouble(prev)/rd/r64/fklBigIntToDouble(&bi);
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
		}
		else
		{
			if(FKL_IS_BIG_INT(prev)&&!FKL_IS_1_BIG_INT(&bi))
			{
				FklBigInt* t=fklCreateBigInt0();
				fklSetBigInt(t,prev->u.bigInt);
				fklDivBigInt(t,&bi);
				fklDivBigIntI(t,r64);
				if(fklIsGtLtFixBigInt(t))
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
		fklUninitBigInt(&bi);
	}
	fklNiEnd(&ap,stack);
}

static void builtin_mod(FKL_DL_PROC_ARGL)
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
		if(!islessgreater(as,0.0))
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
		double r=fmod(af,as);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else if(FKL_IS_FIX(fir)&&FKL_IS_FIX(sec))
	{
		int64_t si=fklGetInt(sec);
		int64_t r=fklGetInt(fir)%si;
		if(si==0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
		fklNiReturn(fklMakeVMint(r,exe),&ap,stack);
	}
	else
	{
		FklBigInt rem=FKL_BIG_INT_INIT;
		fklInitBigInt0(&rem);
		if(FKL_IS_BIG_INT(fir))
			fklSetBigInt(&rem,fir->u.bigInt);
		else
			fklSetBigIntI(&rem,fklGetInt(fir));
		if(FKL_IS_BIG_INT(sec))
		{
			if(FKL_IS_0_BIG_INT(sec->u.bigInt))
			{
				fklUninitBigInt(&rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
			}
			fklModBigInt(&rem,sec->u.bigInt);
		}
		else
		{
			int64_t si=fklGetInt(sec);
			if(si==0)
			{
				fklUninitBigInt(&rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
			}
			fklModBigIntI(&rem,si);
		}
		if(fklIsGtLtFixBigInt(&rem))
		{
			FklBigInt* r=create_uninit_big_int();
			*r=rem;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
		}
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(&rem),exe),&ap,stack);
			fklUninitBigInt(&rem);
		}
	}
	fklNiEnd(&ap,stack);
}

static void builtin_eqn(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	int err=0;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_TOOFEWARG,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)==0;
			if(err)
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

static void builtin_gt(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int r=1;
	int err=0;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_TOOFEWARG,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)>0;
			if(err)
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

static void builtin_ge(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int err=0;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_TOOFEWARG,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)>=0;
			if(err)
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

static void builtin_lt(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int err=0;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_TOOFEWARG,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)<0;
			if(err)
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

static void builtin_le(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	int err=0;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_TOOFEWARG,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)<=0;
			if(err)
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

static void builtin_char_to_integer(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.char->integer",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_CHR,"builtin.char->integer",exe);
	fklNiReturn(FKL_MAKE_VM_FIX(FKL_GET_CHR(obj)),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_integer_to_char(FKL_DL_PROC_ARGL)
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

static void builtin_list_to_vector(FKL_DL_PROC_ARGL)
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

static void builtin_string_to_vector(FKL_DL_PROC_ARGL)
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

static void builtin_make_list(FKL_DL_PROC_ARGL)
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

static void builtin_string_to_list(FKL_DL_PROC_ARGL)
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

static void builtin_bytevector_to_s8_list(FKL_DL_PROC_ARGL)
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
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_FIX(s8a[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_bytevector_to_u8_list(FKL_DL_PROC_ARGL)
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
		fklSetRef(&(*cur)->u.pair->car,FKL_MAKE_VM_FIX(u8a[i]),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_bytevector_to_s8_vector(FKL_DL_PROC_ARGL)
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
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_FIX(s8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_bytevector_to_u8_vector(FKL_DL_PROC_ARGL)
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
		fklSetRef(&vec->u.vec->base[i],FKL_MAKE_VM_FIX(u8a[i]),gc);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_vector_to_list(FKL_DL_PROC_ARGL)
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
		fklDropTop(stack);
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_string(FKL_DL_PROC_ARGL)
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

static void builtin_make_string(FKL_DL_PROC_ARGL)
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

static void builtin_make_vector(FKL_DL_PROC_ARGL)
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

static void builtin_substring(FKL_DL_PROC_ARGL)
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

static void builtin_sub_string(FKL_DL_PROC_ARGL)
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

static void builtin_subbytevector(FKL_DL_PROC_ARGL)
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

static void builtin_sub_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_subvector(FKL_DL_PROC_ARGL)
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

static void builtin_sub_vector(FKL_DL_PROC_ARGL)
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

static void builtin_to_string(FKL_DL_PROC_ARGL)
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
	else if(FKL_IS_USERDATA(obj)&&fklIsAbleToStringUd(obj->u.ud))
		retval=fklCreateVMvalueToStack(FKL_TYPE_STR,fklUdToString(obj->u.ud),exe);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.->string",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_symbol_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_string_to_symbol(FKL_DL_PROC_ARGL)
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

static void builtin_symbol_to_integer(FKL_DL_PROC_ARGL)
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

static void builtin_string_to_number(FKL_DL_PROC_ARGL)
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
			FklBigInt bi=FKL_BIG_INT_INIT;
			fklInitBigIntFromString(&bi,obj->u.str);
			if(!fklIsGtLtFixBigInt(&bi))
			{
				r=FKL_MAKE_VM_FIX(fklBigIntToI64(&bi));
				fklUninitBigInt(&bi);
			}
			else
			{
				FklBigInt* rr=create_uninit_big_int();
				*rr=bi;
				r=fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,rr,exe);
			}
		}
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_number_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_integer_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_f64_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_vector_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_bytevector_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_string_to_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_vector_to_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_list_to_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_list_to_string(FKL_DL_PROC_ARGL)
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

static void builtin_number_to_f64(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.integer->f64",FKL_ERR_TOOFEWARG,exe);
	FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_F64,NULL,exe);
	FKL_NI_CHECK_TYPE(obj,fklIsVMnumber,"builtin.integer->f64",exe);
	if(FKL_IS_FIX(obj))
		retval->u.f64=(double)fklGetInt(obj);
	else if(FKL_IS_BIG_INT(obj))
		retval->u.f64=fklBigIntToDouble(obj->u.bigInt);
	else
		retval->u.f64=obj->u.f64;
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

#include<math.h>

static void builtin_number_to_integer(FKL_DL_PROC_ARGL)
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
		if(isgreater(obj->u.f64,(double)FKL_FIX_INT_MAX)||isless(obj->u.f64,FKL_FIX_INT_MIN))
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCreateBigIntD(obj->u.f64),exe),&ap,stack);
		else
			fklNiReturn(fklMakeVMintD(obj->u.f64,exe),&ap,stack);
	}
	else if(FKL_IS_BIG_INT(obj))
	{
		if(fklIsGtLtFixBigInt(obj->u.bigInt))
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

static void builtin_nth(FKL_DL_PROC_ARGL)
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

static void builtin_set_nth(FKL_DL_PROC_ARGL)
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

static void builtin_sref(FKL_DL_PROC_ARGL)
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

static void builtin_bvs8ref(FKL_DL_PROC_ARGL) {BV_U_S_8_REF(int8_t,"builtin.bvs8ref")}
static void builtin_bvs16ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(int16_t,"builtin.bvs16ref")}
static void builtin_bvs32ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(int32_t,"builtin.bvs32ref")}
static void builtin_bvs64ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(int64_t,"builtin.bvs64ref")}

static void builtin_bvu8ref(FKL_DL_PROC_ARGL) {BV_U_S_8_REF(uint8_t,"builtin.bvu8ref")}
static void builtin_bvu16ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(uint16_t,"builtin.bvu16ref")}
static void builtin_bvu32ref(FKL_DL_PROC_ARGL) {BV_LT_U64_REF(uint32_t,"builtin.bvu32ref")}
static void builtin_bvu64ref(FKL_DL_PROC_ARGL)
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

static void builtin_bvf32ref(FKL_DL_PROC_ARGL) BV_F_REF(float,"builtin.bvf32ref")
static void builtin_bvf64ref(FKL_DL_PROC_ARGL) BV_F_REF(double,"builtin.bvf32ref")
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

static void builtin_set_bvs8ref(FKL_DL_PROC_ARGL) {SET_BV_LE_U8_REF(int8_t,"builtin.set-bvs8ref!")}
static void builtin_set_bvs16ref(FKL_DL_PROC_ARGL) {SET_BV_REF(int16_t,"builtin.set-bvs16ref!")}
static void builtin_set_bvs32ref(FKL_DL_PROC_ARGL) {SET_BV_REF(int32_t,"builtin.set-bvs32ref!")}
static void builtin_set_bvs64ref(FKL_DL_PROC_ARGL) {SET_BV_REF(int64_t,"builtin.set-bvs64ref!")}

static void builtin_set_bvu8ref(FKL_DL_PROC_ARGL) {SET_BV_LE_U8_REF(uint8_t,"builtin.set-bvu8ref!")}
static void builtin_set_bvu16ref(FKL_DL_PROC_ARGL) {SET_BV_REF(uint16_t,"builtin.set-bvu16ref!")}
static void builtin_set_bvu32ref(FKL_DL_PROC_ARGL) {SET_BV_REF(uint32_t,"builtin.set-bvu32ref!")}
static void builtin_set_bvu64ref(FKL_DL_PROC_ARGL) {SET_BV_REF(uint64_t,"builtin.set-bvu64ref!")}
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

static void builtin_set_bvf32ref(FKL_DL_PROC_ARGL) SET_BV_F_REF(float,"builtin.set-bvf32ref!")
static void builtin_set_bvf64ref(FKL_DL_PROC_ARGL) SET_BV_F_REF(double,"builtin.set-bvf64ref!")
#undef SET_BV_F_REF

static void builtin_set_sref(FKL_DL_PROC_ARGL)
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

static void builtin_fill_string(FKL_DL_PROC_ARGL)
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

static void builtin_fill_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_vref(FKL_DL_PROC_ARGL)
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

static void builtin_set_vref(FKL_DL_PROC_ARGL)
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

static void builtin_fill_vector(FKL_DL_PROC_ARGL)
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

static void builtin_cas_vref(FKL_DL_PROC_ARGL)
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

static void builtin_nthcdr(FKL_DL_PROC_ARGL)
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

static void builtin_tail(FKL_DL_PROC_ARGL)
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

static void builtin_set_nthcdr(FKL_DL_PROC_ARGL)
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

static void builtin_length(FKL_DL_PROC_ARGL)
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
	else if(FKL_IS_USERDATA(obj)&&fklUdHasLength(obj->u.ud))
		len=fklLengthVMudata(obj->u.ud);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.length",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(fklMakeVMuint(len,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_fopen(FKL_DL_PROC_ARGL)
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

static void builtin_fclose(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(fp,FKL_IS_FP,"builtin.fclose",exe);
	if(fp->u.fp->mutex||fp->u.fp==NULL||fklDestroyVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fclose",FKL_ERR_INVALIDACCESS,exe);
	fp->u.fp=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

#include"utstring.h"
typedef struct
{
	FklVMvalue* fpv;
	FklStringMatchPattern* head;
	FklStringMatchRouteNode* root;
	FklStringMatchRouteNode* route;
	FklStringMatchSet* matchSet;
	UT_string* buf;
	FklPtrStack* tokenStack;
	const FklSid_t* headSymbol;
	uint32_t ap;
	uint32_t done;
	size_t j;
}ReadCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReadCtx);

static void do_nothing_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
}

static void read_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
	ReadCtx* c=(ReadCtx*)data;
	fklGC_toGrey(c->fpv,gc);
}

static void read_frame_finalizer(FklCallObjData data)
{
	ReadCtx* c=(ReadCtx*)data;
	fklUnLockVMfp(c->fpv);
	utstring_free(c->buf);
	FklPtrStack* s=c->tokenStack;
	while(!fklIsPtrStackEmpty(s))
		fklDestroyToken(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
	fklDestroyStringMatchRoute(c->root);
	fklDestroyStringMatchSet(c->matchSet);
}

static int read_frame_end(FklCallObjData d)
{
	return ((ReadCtx*)d)->done;
}

#include<fcntl.h>

static void read_frame_step(FklCallObjData d,FklVM* exe)
{
	ReadCtx* rctx=(ReadCtx*)d;
	FklVMfp* vfp=rctx->fpv->u.fp;
	FILE* fp=vfp->fp;
	int fd=fileno(fp);
	int attr=fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL,attr|O_NONBLOCK);
	UT_string* s=rctx->buf;
	int ch;
	while((ch=fgetc(fp))>0)
	{
		char c=ch;
		utstring_bincpy(s,&c,sizeof(c));
		if(ch=='\n')
			break;
	}
	fcntl(fd,F_SETFL,attr);
	if(feof(fp)||ch=='\n')
	{
		size_t line=1;
		int err=0;
		rctx->matchSet=fklSplitStringIntoTokenWithPattern(utstring_body(s)
				,utstring_len(s)
				,line
				,&line
				,rctx->j
				,&rctx->j
				,rctx->tokenStack
				,rctx->matchSet
				,rctx->head
				,rctx->route
				,&rctx->route
				,&err);
		if(rctx->matchSet==NULL)
		{
			size_t j=rctx->j;
			size_t len=utstring_len(s);
			size_t errorLine=0;
			if(len-j)
				fklRewindStream(fp,utstring_body(s)+j,len-j);
			rctx->done=1;
			FklNastNode* node=fklCreateNastNodeFromTokenStackAndMatchRoute(rctx->tokenStack
					,rctx->root
					,&errorLine
					,rctx->headSymbol
					,NULL
					,exe->symbolTable);
			if(node==NULL)
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
			FklVMvalue* v=fklCreateVMvalueFromNastNodeAndStoreInStack(node,NULL,exe);
			uint32_t* pap=&rctx->ap;
			fklNiReturn(v,pap,exe->stack);
			fklNiEnd(pap,exe->stack);
			fklDestroyNastNode(node);
		}
		else if(feof(fp))
		{
			rctx->done=1;
			if(rctx->matchSet!=FKL_STRING_PATTERN_UNIVERSAL_SET)
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTEOF,exe);
			else
			{
				uint32_t* pap=&rctx->ap;
				fklNiReturn(FKL_VM_NIL,pap,exe->stack);
				fklNiEnd(pap,exe->stack);
			}
		}
		else if(err)
		{
			rctx->done=1;
			FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
		}
	}
}

static const FklVMframeContextMethodTable ReadContextMethodTable=
{
	.atomic=read_frame_atomic,
	.finalizer=read_frame_finalizer,
	.copy=NULL,
	.print_backtrace=do_nothing_print_backtrace,
	.step=read_frame_step,
	.end=read_frame_end,
};

static inline void initReadCtx(FklCallObjData data
		,FklVMvalue* fpv
		,FklStringMatchPattern* patterns
		,uint32_t ap
		,const FklSid_t headSymbol[4])
{
	ReadCtx* ctx=(ReadCtx*)data;
	ctx->fpv=fpv;
	ctx->head=patterns;
	ctx->matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	ctx->headSymbol=headSymbol;
	ctx->j=0;
	ctx->root=fklCreateStringMatchRouteNode(NULL
			,0,0
			,NULL
			,NULL
			,NULL);
	ctx->route=ctx->root;
	ctx->ap=ap;
	utstring_new(ctx->buf);
	ctx->tokenStack=fklCreatePtrStack(16,16);
	ctx->done=0;
}

static inline void initFrameToReadFrame(FklVM* exe
		,FklVMvalue* fpv
		,FklStringMatchPattern* patterns
		,uint32_t ap
		,const FklSid_t headSymbol[4])
{
	fklLockVMfp(fpv,exe);
	FklVMframe* f=exe->frames;
	f->type=FKL_FRAME_OTHEROBJ;
	f->u.o.t=&ReadContextMethodTable;
	initReadCtx(f->u.o.data,fpv,patterns,ap,headSymbol);
}

static void builtin_read(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_TOOMANYARG,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_DECL_UD_DATA(pbd,PublicBuiltInData,pd->u.ud);
	if(!stream||FKL_IS_FP(stream))
	{
		FklVMvalue* fpv=stream?stream:pbd->sysIn;
		if(!fpv->u.fp)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.read",FKL_ERR_INVALIDACCESS,exe);
		initFrameToReadFrame(exe
				,fpv
				,pbd->patterns
				,ap
				,pbd->builtInHeadSymbolTable);
	}
}

static void builtin_stringify(FKL_DL_PROC_ARGL)
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

static void builtin_parse(FKL_DL_PROC_ARGL)
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
	FKL_DECL_UD_DATA(pbd,PublicBuiltInData,pd->u.ud);
	FklStringMatchPattern* patterns=pbd->patterns;
	FklStringMatchRouteNode* route=fklCreateStringMatchRouteNode(NULL,0,0,NULL,NULL,NULL);
	FklStringMatchRouteNode* troute=route;
	int err=0;
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
			,&troute
			,&err);
	fklDestroyStringMatchSet(matchSet);
	size_t errorLine=0;
	FklNastNode* node=fklCreateNastNodeFromTokenStackAndMatchRoute(&tokenStack
			,route
			,&errorLine
			,pbd->builtInHeadSymbolTable
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

typedef enum
{
	FGETC=0,
	FGETI,
	FGETS,
	FGETB,
	FGETD,
}FgetMode;

typedef struct
{
	FklVMvalue* fpv;
	UT_string buf;
	FgetMode mode:16;
	uint16_t done;
	uint32_t ap;
	size_t len;
	char delim;
}FgetCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(FgetCtx);

static inline void initFgetCtx(FklCallObjData d
		,FklVMvalue* fpv
		,FgetMode mode
		,size_t len
		,char del
		,uint32_t ap)
{
	FgetCtx* ctx=(FgetCtx*)d;
	ctx->fpv=fpv;
	ctx->mode=mode;
	ctx->len=len;
	ctx->ap=ap;
	ctx->done=0;
	ctx->delim=del;
	utstring_init(&ctx->buf);
}

static void fget_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
	FgetCtx* c=(FgetCtx*)data;
	fklGC_toGrey(c->fpv,gc);
}

static void fget_frame_finalizer(FklCallObjData data)
{
	FgetCtx* c=(FgetCtx*)data;
	utstring_done(&c->buf);
	fklUnLockVMfp(c->fpv);
}

static int fget_frame_end(FklCallObjData d)
{
	return ((FgetCtx*)d)->done;
}

static void fget_frame_step(FklCallObjData d,FklVM* exe)
{
	FgetCtx* ctx=(FgetCtx*)d;
	FklVMfp* vfp=ctx->fpv->u.fp;
	FILE* fp=vfp->fp;
	int fd=fileno(fp);
	int attr=fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL,attr|O_NONBLOCK);
	int ch=0;
	UT_string* s=&ctx->buf;
	switch(ctx->mode)
	{
		case FGETC:
		case FGETI:
			ch=fgetc(fp);
			if(feof(fp))
			{
				ctx->done=1;
				uint32_t* pap=&ctx->ap;
				fklNiReturn(FKL_VM_NIL,pap,exe->stack);
				fklNiEnd(pap,exe->stack);
			}
			else if(ch>0)
			{
				ctx->done=1;
				FklVMvalue* retval=ctx->mode==FGETC?FKL_MAKE_VM_CHR(ch):FKL_MAKE_VM_FIX(ch);
				uint32_t* pap=&ctx->ap;
				fklNiReturn(retval,pap,exe->stack);
				fklNiEnd(pap,exe->stack);
			}
			break;
		case FGETD:
			while((ch=fgetc(fp))>0)
			{
				char c=ch;
				utstring_bincpy(s,&c,sizeof(c));
				if(ch==ctx->delim)
					break;
			}
			if(feof(fp)||ch==ctx->delim)
			{
				ctx->done=1;
				FklVMvalue* retval=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(utstring_len(s),utstring_body(s)),exe);
				uint32_t* pap=&ctx->ap;
				fklNiReturn(retval,pap,exe->stack);
				fklNiEnd(pap,exe->stack);
			}
			break;
		case FGETS:
		case FGETB:
			while(ctx->len&&(ch=fgetc(fp))>0)
			{
				char c=ch;
				utstring_bincpy(s,&c,sizeof(c));
				ctx->len--;
			}
			if(!ctx->len||feof(fp))
			{
				ctx->done=1;
				uint32_t* pap=&ctx->ap;
				size_t len=utstring_len(s);
				const char* body=utstring_body(s);
				FklVMvalue* retval=ctx->mode==FGETS?fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(len,body),exe)
					:fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(len,(const uint8_t*)body),exe);
				fklNiReturn(retval,pap,exe->stack);
				fklNiEnd(pap,exe->stack);
			}
			break;
	}
	fcntl(fd,F_SETFL,attr);
}

static const FklVMframeContextMethodTable FgetContextMethodTable=
{
	.atomic=fget_frame_atomic,
	.finalizer=fget_frame_finalizer,
	.copy=NULL,
	.print_backtrace=do_nothing_print_backtrace,
	.step=fget_frame_step,
	.end=fget_frame_end,
};

static inline void initFrameToFgetFrame(FklVM* exe
		,FklVMvalue* fpv
		,FgetMode mode
		,size_t len
		,char del
		,uint32_t ap)
{
	FklVMframe* f=exe->frames;
	fklLockVMfp(fpv,exe);
	f->type=FKL_FRAME_OTHEROBJ;
	f->u.o.t=&FgetContextMethodTable;
	initFgetCtx(f->u.o.data,fpv,mode,len,del,ap);
}

static void builtin_fgetd(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* del=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetd",FKL_ERR_TOOMANYARG,exe);
	if(!del)
		del=FKL_MAKE_VM_CHR('\n');
	if(!file)
		file=FKL_GET_UD_DATA(PublicBuiltInData,pd->u.ud)->sysIn;
	if(!FKL_IS_FP(file)||!FKL_IS_CHR(del))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetd",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(!file->u.fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetd",FKL_ERR_INVALIDACCESS,exe);
	initFrameToFgetFrame(exe,file,FGETD,0,FKL_GET_CHR(del),ap);
}

static void builtin_fgets(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOMANYARG,exe);
	if(!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_TOOFEWARG,exe);
	if(!file)
		file=FKL_GET_UD_DATA(PublicBuiltInData,pd->u.ud)->sysIn;
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	if(!file->u.fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgets",FKL_ERR_INVALIDACCESS,exe);
	initFrameToFgetFrame(exe,file,FGETS,fklGetUint(psize),0,ap);
}

static void builtin_fgetb(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOMANYARG,exe);
	if(!psize)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_TOOFEWARG,exe);
	if(!file)
		file=FKL_GET_UD_DATA(PublicBuiltInData,pd->u.ud)->sysIn;
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	if(!file->u.fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetb",FKL_ERR_INVALIDACCESS,exe);
	initFrameToFgetFrame(exe,file,FGETB,fklGetUint(psize),0,ap);
}

static void builtin_prin1(FKL_DL_PROC_ARGL)
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

static void builtin_princ(FKL_DL_PROC_ARGL)
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

static void builtin_println(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	for(;obj;obj=fklNiGetArg(&ap,stack))
		fklPrincVMvalue(obj,stdout,exe->symbolTable);
	fputc('\n',stdout);
	fklNiResBp(&ap,stack);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_print(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	for(;obj;obj=fklNiGetArg(&ap,stack))
		fklPrincVMvalue(obj,stdout,exe->symbolTable);
	fklNiResBp(&ap,stack);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_fprint(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* f=fklNiGetArg(&ap,stack);
	if(!f)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fprint",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(f,FKL_IS_FP,"builtin.fprint",exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FILE* fp=f->u.fp->fp;
	for(;obj;obj=fklNiGetArg(&ap,stack))
		fklPrincVMvalue(obj,fp,exe->symbolTable);
	fklNiResBp(&ap,stack);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_fprin1(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* f=fklNiGetArg(&ap,stack);
	if(!f)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fprin1",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(f,FKL_IS_FP,"builtin.fprin1",exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FILE* fp=f->u.fp->fp;
	for(;obj;obj=fklNiGetArg(&ap,stack))
		fklPrin1VMvalue(obj,fp,exe->symbolTable);
	fklNiResBp(&ap,stack);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_newline(FKL_DL_PROC_ARGL)
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

static void builtin_dlopen(FKL_DL_PROC_ARGL)
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

static void builtin_dlsym(FKL_DL_PROC_ARGL)
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

static void builtin_argv(FKL_DL_PROC_ARGL)
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

inline static FklVMvalue* isSlot(const FklVMvalue* head,const FklVMvalue* v)
{
	if(FKL_IS_PAIR(v)
			&&v->u.pair->car==head
			&&FKL_IS_PAIR(v->u.pair->cdr)
			&&v->u.pair->cdr->u.pair->cdr==FKL_VM_NIL
			&&FKL_IS_SYM(v->u.pair->cdr->u.pair->car))
		return v->u.pair->cdr->u.pair->car;
	return NULL;
}

int matchPattern(const FklVMvalue* pattern,FklVMvalue* exp,FklHashTable* ht,FklVMgc* gc)
{
	FklVMvalue* slotS=pattern->u.pair->car;
	FklPtrQueue q0={NULL,NULL,};
	FklPtrQueue q1={NULL,NULL,};
	fklInitPtrQueue(&q0);
	fklInitPtrQueue(&q1);
	fklPushPtrQueue(pattern->u.pair->cdr->u.pair->car,&q0);
	fklPushPtrQueue(exp,&q1);
	int r=0;
	while(!fklIsPtrQueueEmpty(&q0)&&!fklIsPtrQueueEmpty(&q1))
	{
		FklVMvalue* v0=fklPopPtrQueue(&q0);
		FklVMvalue* v1=fklPopPtrQueue(&q1);
		FklVMvalue* slotV=isSlot(slotS,v0);
		if(slotV)
			fklSetVMhashTable(slotV,v1,ht,gc);
		else if(FKL_IS_BOX(v0)&&FKL_IS_BOX(v1))
		{
			fklPushPtrQueue(v0->u.box,&q0);
			fklPushPtrQueue(v1->u.box,&q1);
		}
		else if(FKL_IS_PAIR(v0)&&FKL_IS_PAIR(v1))
		{
			fklPushPtrQueue(v0->u.pair->car,&q0);
			fklPushPtrQueue(v0->u.pair->cdr,&q0);
			fklPushPtrQueue(v1->u.pair->car,&q1);
			fklPushPtrQueue(v1->u.pair->cdr,&q1);
		}
		else if(FKL_IS_VECTOR(v0)
				&&FKL_IS_VECTOR(v1))
		{
			r=v0->u.vec->size!=v1->u.vec->size;
			if(r)
				break;
			FklVMvalue** b0=v0->u.vec->base;
			FklVMvalue** b1=v1->u.vec->base;
			size_t size=v0->u.vec->size;
			for(size_t i=0;i<size;i++)
			{
				fklPushPtrQueue(b0[i],&q0);
				fklPushPtrQueue(b1[i],&q1);
			}
		}
		else if(FKL_IS_HASHTABLE(v0)
				&&FKL_IS_HASHTABLE(v1))
		{
			r=v0->u.hash->t!=v1->u.hash->t
				||v0->u.hash->num!=v1->u.hash->num;
			if(r)
				break;
			FklHashTableNodeList* h0=v0->u.hash->list;
			FklHashTableNodeList* h1=v1->u.hash->list;
			while(h0)
			{
				FklVMhashTableItem* i0=(FklVMhashTableItem*)h0->node->data;
				FklVMhashTableItem* i1=(FklVMhashTableItem*)h1->node->data;
				fklPushPtrQueue(i0->key,&q0);
				fklPushPtrQueue(i0->v,&q0);
				fklPushPtrQueue(i1->key,&q1);
				fklPushPtrQueue(i1->v,&q1);
				h0=h0->next;
				h1=h1->next;
			}
		}
		else if(!fklVMvalueEqual(v0,v1))
		{
			r=1;
			break;
		}
	}
	fklUninitPtrQueue(&q0);
	fklUninitPtrQueue(&q1);
	return r;
}

static int isValidSyntaxPattern(const FklVMvalue* p)
{
	if(!FKL_IS_PAIR(p))
		return 0;
	FklVMvalue* head=p->u.pair->car;
	if(!FKL_IS_SYM(head))
		return 0;
	const FklVMvalue* body=p->u.pair->cdr;
	if(!FKL_IS_PAIR(body))
		return 0;
	if(body->u.pair->cdr!=FKL_VM_NIL)
		return 0;
	body=body->u.pair->car;
	FklHashTable* symbolTable=fklCreateSidSet();
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack((void*)body,&stack);
	while(!fklIsPtrStackEmpty(&stack))
	{
		const FklVMvalue* c=fklPopPtrStack(&stack);
		FklVMvalue* slotV=isSlot(head,c);
		if(slotV)
		{
			FklSid_t sid=FKL_GET_SYM(slotV);
			if(fklGetHashItem(&sid,symbolTable))
			{
				fklDestroyHashTable(symbolTable);
				fklUninitPtrStack(&stack);
				return 0;
			}
			fklPutHashItem(&sid,symbolTable);
		}
		if(FKL_IS_PAIR(c))
		{
			fklPushPtrStack(c->u.pair->car,&stack);
			fklPushPtrStack(c->u.pair->cdr,&stack);
		}
		else if(FKL_IS_BOX(c))
			fklPushPtrStack(c->u.box,&stack);
		else if(FKL_IS_VECTOR(c))
		{
			FklVMvalue** base=c->u.vec->base;
			size_t size=c->u.vec->size;
			for(size_t i=0;i<size;i++)
				fklPushPtrStack(base[i],&stack);
		}
		else if(FKL_IS_HASHTABLE(c))
		{
			for(FklHashTableNodeList* h=c->u.hash->list;h;h=h->next)
			{
				FklVMhashTableItem* i=(FklVMhashTableItem*)h->node->data;
				fklPushPtrStack(i->key,&stack);
				fklPushPtrStack(i->v,&stack);
			}
		}
	}
	fklDestroyHashTable(symbolTable);
	fklUninitPtrStack(&stack);
	return 1;
}

static void builtin_pmatch(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* pattern=fklNiGetArg(&ap,stack);
	FklVMvalue* exp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.pmatch",FKL_ERR_TOOFEWARG,exe);
	if(!isValidSyntaxPattern(pattern))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.pmatch",FKL_ERR_INVALIDPATTERN,exe);
	FklHashTable* hash=fklCreateVMhashTableEq();
	if(matchPattern(pattern,exp,hash,exe->gc))
	{
		fklDestroyVMhashTable(hash);
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,hash,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_go(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* threadProc=fklNiGetArg(&ap,stack);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.go",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsCallable(threadProc))
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
	fklNiSetBpWithTp(threadVMstack);
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
	fklNiReturn(chan,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_chanl(FKL_DL_PROC_ARGL)
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

static void builtin_chanl_num(FKL_DL_PROC_ARGL)
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

static void builtin_send(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOMANYARG,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.send",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ch,FKL_IS_CHAN,"builtin.send",exe);
	fklChanlSend(message,ch->u.chan,exe);
	fklNiReturn(message,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_recv(FKL_DL_PROC_ARGL)
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
		fklChanlRecv(fklNiReturn(FKL_VM_NIL,&ap,stack),ch->u.chan,exe);
	fklNiEnd(&ap,stack);
}

static void builtin_error(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* who=fklNiGetArg(&ap,stack);
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

static void builtin_raise(FKL_DL_PROC_ARGL)
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

static void builtin_throw(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* who=fklNiGetArg(&ap,stack);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.throw",FKL_ERR_TOOMANYARG,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.throw",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.throw",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR
			,fklCreateVMerror((FKL_IS_SYM(who))
				?fklGetSymbolWithId(FKL_GET_SYM(who)
					,exe->symbolTable)->symbol
				:who->u.str
				,FKL_GET_SYM(type)
				,fklCopyString(message->u.str)),exe);
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

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(EhFrameContext);

static void error_handler_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
	EhFrameContext* c=(EhFrameContext*)data;
	FklVMdlproc* dlproc=c->proc->u.dlproc;
	if(dlproc->sid)
	{
		fprintf(fp,"at dlproc:");
		fklPrintString(fklGetSymbolWithId(dlproc->sid,table)->symbol
				,fp);
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

static void error_handler_frame_copy(FklCallObjData d,const FklCallObjData s,FklVM* exe)
{
	const EhFrameContext* const sc=(const EhFrameContext*)s;
	EhFrameContext* dc=(EhFrameContext*)d;
	FklVMgc* gc=exe->gc;
	fklSetRef(&dc->proc,sc->proc,gc);
	size_t num=sc->num;
	dc->num=num;
	dc->errorSymbolLists=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*num);
	FKL_ASSERT(dc->errorSymbolLists||!num);
	dc->errorHandlers=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*num);
	FKL_ASSERT(dc->errorHandlers||!num);
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

static int errorCallBackWithErrorHandler(FklVMframe* f,FklVMvalue* errValue,FklVM* exe)
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
			stack->tp=fklNiSetBp(c->bp,stack);
			fklPushVMvalue(errValue,stack);
			FklVMframe* topFrame=exe->frames;
			exe->frames=f;
			while(topFrame!=f)
			{
				FklVMframe* cur=topFrame;
				topFrame=topFrame->prev;
				fklDestroyVMframe(cur,exe);
			}
			fklTailCallObj(c->errorHandlers[i],f,exe);
			return 1;
		}
	}
	return 0;
}

static void builtin_call_eh(FKL_DL_PROC_ARGL)
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

	ap=fklNiSetBp(ap,stack);
	if(errSymbolLists.top)
	{
		FklVMframe* sf=exe->frames;
		FklVMframe* nf=fklCreateOtherObjVMframe(sf->u.o.t,sf->prev);
		nf->errorCallBack=errorCallBackWithErrorHandler;
		fklDoCopyObjFrameContext(sf,nf,exe);
		exe->frames=nf;
		nf->u.o.t=&ErrorHandlerContextMethodTable;
		EhFrameContext* c=(EhFrameContext*)nf->u.o.data;
		c->num=errSymbolLists.top;
		FklVMvalue** t=(FklVMvalue**)realloc(errSymbolLists.base,errSymbolLists.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorSymbolLists=t;
		t=(FklVMvalue**)realloc(errHandlers.base,errHandlers.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorHandlers=t;
		c->bp=ap;
		fklCallObj(proc,nf,exe);
	}
	else
	{
		fklUninitPtrStack(&errSymbolLists);
		fklUninitPtrStack(&errHandlers);
		fklTailCallObj(proc,exe->frames,exe);
	}
	fklNiEnd(&ap,exe->stack);
#undef GET_PROC
#undef GET_LIST
}

static void builtin_apply(FKL_DL_PROC_ARGL)
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
	ap=fklNiSetBp(ap,stack);
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
	fklTailCallObj(proc,frame,exe);
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
	FklVMframe* frame=exe->frames;\
	if(s==FKL_CC_RE)\
	{\
		FklVMvalue* result=fklGetTopValue(exe->stack);\
		RESULT_PROCESS\
		NEXT_PROCESS\
		fklDropTop(exe->stack);\
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
		return fklCallInDlproc(mapctx->proc,argNum,mapctx->cars->u.vec->base,frame,exe,(K_FUNC),mapctx,sizeof(MapCtx));\
	}\
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
		fklCallFuncK((K_FUNC),exe,mapctx);}\

static void k_map(K_FUNC_ARGL) {K_MAP_PATTERN(k_map,
		*(mapctx->cur)=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),exe);,
		fklSetRef(&(*mapctx->cur)->u.pair->car,result,gc);,
		mapctx->cur=&(*mapctx->cur)->u.pair->cdr;)}
static void builtin_map(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.map",k_map,FKL_VM_NIL)}

static void k_foreach(K_FUNC_ARGL) {K_MAP_PATTERN(k_foreach,,*(mapctx->r)=result;,)}
static void builtin_foreach(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.foreach",k_foreach,FKL_VM_NIL)}

static void k_andmap(K_FUNC_ARGL) {K_MAP_PATTERN(k_andmap,,*(mapctx->r)=result;if(result==FKL_VM_NIL)mapctx->i=len;,)}
static void builtin_andmap(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.andmap",k_andmap,FKL_VM_TRUE)}

static void k_ormap(K_FUNC_ARGL) {K_MAP_PATTERN(k_ormap,,*(mapctx->r)=result;if(result!=FKL_VM_NIL)mapctx->i=len;,)}
static void builtin_ormap(FKL_DL_PROC_ARGL) {MAP_PATTERN("builtin.ormap",k_ormap,FKL_VM_NIL)}

#undef K_MAP_PATTERN
#undef MAP_PATTERN

static void builtin_memq(FKL_DL_PROC_ARGL)
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
	uint32_t ap;
}MemberCtx;

static void k_member(K_FUNC_ARGL)
{
	MemberCtx* memberctx=(MemberCtx*)ctx;
	FklVMstack* stack=exe->stack;
	if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*memberctx->r=memberctx->list;
			memberctx->list=FKL_VM_NIL;
		}
		else
			memberctx->list=memberctx->list->u.pair->cdr;
		fklDropTop(stack);
	}
	if(memberctx->list!=FKL_VM_NIL)
	{
		FklVMvalue* arglist[2]={memberctx->obj,memberctx->list->u.pair->car};
		return fklCallInDlproc(memberctx->proc
				,2,arglist
				,exe->frames,exe,k_member,memberctx,sizeof(MemberCtx));
	}
	fklNiReturn(*memberctx->r,&memberctx->ap,stack);
	fklNiEnd(&memberctx->ap,stack);
}

static void builtin_member(FKL_DL_PROC_ARGL)
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
		fklCallFuncK(k_member,exe,memberctx);
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
	if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*mempctx->r=mempctx->list;
			mempctx->list=FKL_VM_NIL;
		}
		else
			mempctx->list=mempctx->list->u.pair->cdr;
		fklDropTop(stack);
	}
	if(mempctx->list!=FKL_VM_NIL)
	{
		return fklCallInDlproc(mempctx->proc
				,1,&mempctx->list->u.pair->car
				,exe->frames,exe,k_memp,mempctx,sizeof(MempCtx));
	}
	fklNiReturn(*mempctx->r,&mempctx->ap,stack);
	fklNiEnd(&mempctx->ap,stack);
}

static void builtin_memp(FKL_DL_PROC_ARGL)
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
	fklCallFuncK(k_memp,exe,mempctx);
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
	if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklGetTopValue(stack);
		if(result!=FKL_VM_NIL)
		{
			*filterctx->cur=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),exe);
			fklSetRef(&(*filterctx->cur)->u.pair->car,filterctx->list->u.pair->car,exe->gc);
			filterctx->cur=&(*filterctx->cur)->u.pair->cdr;
		}
		filterctx->list=filterctx->list->u.pair->cdr;
		fklDropTop(stack);
	}
	if(filterctx->list!=FKL_VM_NIL)
	{
		return fklCallInDlproc(filterctx->proc
				,1,&filterctx->list->u.pair->car
				,exe->frames,exe,k_filter,filterctx,sizeof(FilterCtx));
	}
	fklNiReturn(*filterctx->r,&filterctx->ap,stack);
	fklNiEnd(&filterctx->ap,stack);
}

static void builtin_filter(FKL_DL_PROC_ARGL)
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
	fklCallFuncK(k_filter,exe,filterctx);
}

static void builtin_list(FKL_DL_PROC_ARGL)
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

static void builtin_list8(FKL_DL_PROC_ARGL)
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

static void builtin_reverse(FKL_DL_PROC_ARGL)
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

static void builtin_feof(FKL_DL_PROC_ARGL)
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
	fklNiReturn(feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_vector(FKL_DL_PROC_ARGL)
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

static void builtin_getcwd(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.getcwd",FKL_ERR_TOOMANYARG,exe);
	FklVMvalue* s=fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateStringFromCstr(fklGetCwd()),exe);
	fklNiReturn(s,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_cd(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* dir=fklNiGetArg(&ap,stack);
	if(!dir)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cd",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.cd",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(dir,FKL_IS_STR,"builtin.cd",exe);
	char* cdir=fklStringToCstr(dir->u.str);
	int r=fklChangeWorkPath(cdir);
	if(r)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("builtin.cd",cdir,1,FKL_ERR_FILEFAILURE,exe);
	free(cdir);
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_fgetc(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_TOOMANYARG,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_DECL_UD_DATA(pbd,PublicBuiltInData,pd->u.ud);
	FklVMvalue* fpv=stream?stream:pbd->sysIn;
	if(!fpv->u.fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgetc",FKL_ERR_INVALIDACCESS,exe);
	initFrameToFgetFrame(exe,fpv,FGETC,1,0,ap);
}

static void builtin_fgeti(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_TOOMANYARG,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_DECL_UD_DATA(pbd,PublicBuiltInData,pd->u.ud);
	FklVMvalue* fpv=stream?stream:pbd->sysIn;
	if(!fpv->u.fp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fgeti",FKL_ERR_INVALIDACCESS,exe);
	initFrameToFgetFrame(exe,fpv,FGETI,1,0,ap);
}

static void builtin_fwrite(FKL_DL_PROC_ARGL)
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
	if(FKL_IS_STR(obj))
		fwrite(obj->u.str->str,obj->u.str->size,1,objFile);
	if(FKL_IS_BYTEVECTOR(obj))
		fwrite(obj->u.bvec->ptr,obj->u.bvec->size,1,objFile);
	else if(FKL_IS_USERDATA(obj)&&fklIsWritableUd(obj->u.ud))
		fklWriteVMudata(obj->u.ud,objFile);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.fwrite",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_box(FKL_DL_PROC_ARGL)
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

static void builtin_unbox(FKL_DL_PROC_ARGL)
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

static void builtin_set_box(FKL_DL_PROC_ARGL)
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

static void builtin_cas_box(FKL_DL_PROC_ARGL)
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

static void builtin_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_make_bytevector(FKL_DL_PROC_ARGL)
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

static void builtin_sleep(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* second=fklNiGetArg(&ap,stack);
	if(!second)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sleep",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(second,fklIsInt,"builtin.sleep",exe);
	uint64_t sec=fklGetUint(second);
	fklSleepThread(exe,sec);
	fklNiReturn(second,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_srand(FKL_DL_PROC_ARGL)
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

static void builtin_rand(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue*  lim=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_TOOMANYARG,exe);
	if(lim&&!fklIsInt(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.rand",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(FKL_MAKE_VM_FIX(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))),&ap,stack);
	fklNiEnd(&ap,stack);
}

#include<time.h>

static void builtin_get_time(FKL_DL_PROC_ARGL)
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

static void builtin_remove_file(FKL_DL_PROC_ARGL)
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
	fklNiReturn(FKL_MAKE_VM_FIX(remove(str)),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_time(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.time",FKL_ERR_TOOMANYARG,exe);
	fklNiReturn(fklMakeVMint((int64_t)time(NULL),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_system(FKL_DL_PROC_ARGL)
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

static void builtin_hash(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklHashTable* ht=fklCreateVMhashTableEq();
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

static void builtin_hash_num(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-num",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-num",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-num",exe);
	fklNiReturn(fklMakeVMuint(ht->u.hash->num,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_make_hash(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklHashTable* ht=fklCreateVMhashTableEq();
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

static void builtin_hasheqv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklHashTable* ht=fklCreateVMhashTableEqv();
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

static void builtin_make_hasheqv(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklHashTable* ht=fklCreateVMhashTableEqv();
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

static void builtin_hashequal(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklHashTable* ht=fklCreateVMhashTableEqual();
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

static void builtin_make_hashequal(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklHashTable* ht=fklCreateVMhashTableEqual();
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

static void builtin_href(FKL_DL_PROC_ARGL)
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

static void builtin_href1(FKL_DL_PROC_ARGL)
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

static void builtin_hash_clear(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-clear!",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.hash-clear!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-clear!",exe);
	fklClearVMhashTable(ht->u.hash,exe->gc);
	fklNiReturn(ht,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_set_href(FKL_DL_PROC_ARGL)
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

static void builtin_set_href8(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href*!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.set-href*!",exe);
	FklVMvalue* value=NULL;
	for(FklVMvalue* key=fklNiGetArg(&ap,stack);key;key=fklNiGetArg(&ap,stack))
	{
		value=fklNiGetArg(&ap,stack);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.set-href*!",FKL_ERR_TOOFEWARG,exe);
		fklSetVMhashTable(key,value,ht->u.hash,exe->gc);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(value,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_hash_to_list(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash->list",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash->list",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash->list",exe);
	FklHashTable* hash=ht->u.hash;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableNodeList* list=hash->list;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->node->data;
		FklVMvalue* pair=fklCreateVMpairV(item->key,item->v,exe);
		fklSetRef(cur,fklCreateVMpairV(pair,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_hash_keys(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-keys",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-keys",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-keys",exe);
	FklHashTable* hash=ht->u.hash;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableNodeList* list=hash->list;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->node->data;
		fklSetRef(cur,fklCreateVMpairV(item->key,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_hash_values(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklVMvalue* ht=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-values",FKL_ERR_TOOMANYARG,exe);
	if(!ht)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builin.hash-values",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(ht,FKL_IS_HASHTABLE,"builtin.hash-values",exe);
	FklHashTable* hash=ht->u.hash;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableNodeList* list=hash->list;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->node->data;
		fklSetRef(cur,fklCreateVMpairV(item->v,FKL_VM_NIL,exe),gc);
		cur=&(*cur)->u.pair->cdr;
	}
	fklNiReturn(r,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void builtin_not(FKL_DL_PROC_ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.not")}
static void builtin_null(FKL_DL_PROC_ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.null")}
static void builtin_atom(FKL_DL_PROC_ARGL) {PREDICATE(!FKL_IS_PAIR(val),"builtin.atom?")}
static void builtin_char_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_CHR(val),"builtin.char?")}
static void builtin_integer_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_FIX(val)||FKL_IS_BIG_INT(val),"builtin.integer?")}
static void builtin_fix_int_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_FIX(val),"builtin.fix-int?")}
static void builtin_f64_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_F64(val),"builtin.i64?")}
static void builtin_number_p(FKL_DL_PROC_ARGL) {PREDICATE(fklIsVMnumber(val),"builtin.number?")}
static void builtin_pair_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_PAIR(val),"builtin.pair?")}
static void builtin_symbol_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_SYM(val),"builtin.symbol?")}
static void builtin_string_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_STR(val),"builtin.string?")}
static void builtin_error_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_ERR(val),"builtin.error?")}
static void builtin_procedure_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_PROC(val)||FKL_IS_DLPROC(val),"builtin.procedure?")}
static void builtin_proc_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_PROC(val),"builtin.proc?")}
static void builtin_dlproc_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_DLPROC(val),"builtin.dlproc?")}
static void builtin_vector_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_VECTOR(val),"builtin.vector?")}
static void builtin_bytevector_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_BYTEVECTOR(val),"builtin.bytevector?")}
static void builtin_chanl_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_CHAN(val),"builtin.chanl?")}
static void builtin_dll_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_DLL(val),"builtin.dll?")}
static void builtin_big_int_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_BIG_INT(val),"builtin.big-int?")}
static void builtin_list_p(FKL_DL_PROC_ARGL){PREDICATE(fklIsList(val),"builtin.list?")}
static void builtin_box_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_BOX(val),"builtin.box?")}
static void builtin_hash_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE(val),"builtin.hash?")}
static void builtin_hasheq_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQ(val),"builtin.hash?")}
static void builtin_hasheqv_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQV(val),"builtin.hash?")}
static void builtin_hashequal_p(FKL_DL_PROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQUAL(val),"builtin.hash?")}

static void builtin_odd_p(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.odd?",FKL_ERR_TOOMANYARG,exe);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.odd?",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(val,fklIsInt,"builtin.odd?",exe);
	int r=0;
	if(FKL_IS_FIX(val))
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

static void builtin_even_p(FKL_DL_PROC_ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.even?",FKL_ERR_TOOMANYARG,exe);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.even?",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(val,fklIsInt,"builtin.even?",exe);
	int r=0;
	if(FKL_IS_FIX(val))
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

#include<fakeLisp/opcode.h>

#define INL_FUNC_ARGS FklByteCodelnt* bcs[],FklSid_t fid,uint64_t line

static inline FklByteCodelnt* inl_0_arg_func(FklOpcode opc,FklSid_t fid,uint64_t line)
{
	uint8_t op[1]={opc};
	FklByteCode bc={1,op};
	FklByteCodelnt* r=fklCreateByteCodelnt(fklCreateByteCode(0));
	fklBclBcAppendToBcl(r,&bc,fid,line);
	return r;
}

static inline FklByteCodelnt* inlfunc_add0(INL_FUNC_ARGS)
{
	return inl_0_arg_func(FKL_OP_PUSH_0,fid,line);
}

static inline FklByteCodelnt* inlfunc_mul0(INL_FUNC_ARGS)
{
	return inl_0_arg_func(FKL_OP_PUSH_1,fid,line);
}

static inline FklByteCodelnt* inl_1_arg_func(FklOpcode opc,FklByteCodelnt* bcs[],FklSid_t fid,uint64_t line)
{
	uint8_t op[1]={opc};
	FklByteCode bc={1,op};
	fklBclBcAppendToBcl(bcs[0],&bc,fid,line);
	return bcs[0];
}

static FklByteCodelnt* inlfunc_true(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_TRUE,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_not(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_NOT,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_car(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_PUSH_CAR,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_cdr(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_PUSH_CDR,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_add_1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_INC,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_sub_1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_DEC,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_neg(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_NEG,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_rec(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_REC,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_add1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_ADD1,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_mul1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_MUL1,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_box(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_BOX,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_unbox(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_UNBOX,bcs,fid,line);
}

static inline FklByteCodelnt* inl_2_arg_func(FklOpcode opc,FklByteCodelnt* bcs[],FklSid_t fid,uint64_t line)
{
	uint8_t op[1]={opc};
	FklByteCode bc={1,op};
	fklReCodeLntCat(bcs[1],bcs[0]);
	fklBclBcAppendToBcl(bcs[0],&bc,fid,line);
	fklDestroyByteCodelnt(bcs[1]);
	return bcs[0];
}

static FklByteCodelnt* inlfunc_cons(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_CONS,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_eq(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQ,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_eqv(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQV,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_equal(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQUAL,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_eqn(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQN,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_mul(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_MUL,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_div(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_DIV,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_idiv(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_IDIV,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_mod(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_MOD,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_nth(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_NTH,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_vref(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_VEC_REF,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_sref(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_STR_REF,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_add(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_ADD,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_sub(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_SUB,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_gt(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_GT,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_lt(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_LT,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_ge(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_GE,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_le(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_LE,bcs,fid,line);
}

static inline FklByteCodelnt* inl_3_arg_func(FklOpcode opc,FklByteCodelnt* bcs[],FklSid_t fid,uint64_t line)
{
	uint8_t op[1]={opc};
	FklByteCode bc={1,op};
	fklReCodeLntCat(bcs[1],bcs[0]);
	fklReCodeLntCat(bcs[2],bcs[0]);
	fklBclBcAppendToBcl(bcs[0],&bc,fid,line);
	fklDestroyByteCodelnt(bcs[1]);
	fklDestroyByteCodelnt(bcs[2]);
	return bcs[0];
}

static FklByteCodelnt* inlfunc_eqn3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_EQN3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_gt3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_GT3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_lt3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_LT3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_ge3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_GE3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_le3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_LE3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_add3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_ADD3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_sub3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_SUB3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_mul3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_MUL3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_div3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_DIV3,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_idiv3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_IDIV3,bcs,fid,line);
}

#undef INL_FUNC_ARGS

static const struct SymbolFuncStruct
{
	const char* s;
	FklVMdllFunc f;
	FklBuiltinInlineFunc inlfunc[4];
}builtInSymbolList[]=
{
	{"stdin",                 NULL,                            {NULL,         NULL,          NULL,          NULL,          }, },
	{"stdout",                NULL,                            {NULL,         NULL,          NULL,          NULL,          }, },
	{"stderr",                NULL,                            {NULL,         NULL,          NULL,          NULL,          }, },
	{"car",                   builtin_car,                     {NULL,         inlfunc_car,   NULL,          NULL,          }, },
	{"cdr",                   builtin_cdr,                     {NULL,         inlfunc_cdr,   NULL,          NULL,          }, },
	{"cons",                  builtin_cons,                    {NULL,         NULL,          inlfunc_cons,  NULL,          }, },
	{"append",                builtin_append,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"append!",               builtin_append1,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"copy",                  builtin_copy,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"atom",                  builtin_atom,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"null",                  builtin_null,                    {NULL,         inlfunc_not,   NULL,          NULL,          }, },
	{"not",                   builtin_not,                     {NULL,         inlfunc_not,   NULL,          NULL,          }, },
	{"eq",                    builtin_eq,                      {NULL,         NULL,          inlfunc_eq,    NULL,          }, },
	{"eqv",                   builtin_eqv,                     {NULL,         NULL,          inlfunc_eqv,   NULL,          }, },
	{"equal",                 builtin_equal,                   {NULL,         NULL,          inlfunc_equal, NULL,          }, },
	{"=",                     builtin_eqn,                     {NULL,         inlfunc_true,  inlfunc_eqn,   inlfunc_eqn3,  }, },
	{"+",                     builtin_add,                     {inlfunc_add0, inlfunc_add1,  inlfunc_add,   inlfunc_add3,  }, },
	{"1+",                    builtin_add_1,                   {NULL,         inlfunc_add_1, NULL,          NULL,          }, },
	{"-",                     builtin_sub,                     {NULL,         inlfunc_neg,   inlfunc_sub,   inlfunc_sub3,  }, },
	{"-1+",                   builtin_sub_1,                   {NULL,         inlfunc_sub_1, NULL,          NULL,          }, },
	{"*",                     builtin_mul,                     {inlfunc_mul0, inlfunc_mul1,  inlfunc_mul,   inlfunc_mul3,  }, },
	{"/",                     builtin_div,                     {NULL,         inlfunc_rec,   inlfunc_div,   inlfunc_div3,  }, },
	{"//",                    builtin_idiv,                    {NULL,         NULL,          inlfunc_idiv,  inlfunc_idiv3, }, },
	{"%",                     builtin_mod,                     {NULL,         NULL,          inlfunc_mod,   NULL,          }, },
	{"abs",                   builtin_abs,                     {NULL,         NULL,          NULL,          NULL,          }, },
	{">",                     builtin_gt,                      {NULL,         inlfunc_true,  inlfunc_gt,    inlfunc_gt3,   }, },
	{">=",                    builtin_ge,                      {NULL,         inlfunc_true,  inlfunc_ge,    inlfunc_ge3,   }, },
	{"<",                     builtin_lt,                      {NULL,         inlfunc_true,  inlfunc_lt,    inlfunc_lt3,   }, },
	{"<=",                    builtin_le,                      {NULL,         inlfunc_true,  inlfunc_le,    inlfunc_le3,   }, },
	{"nth",                   builtin_nth,                     {NULL,         NULL,          inlfunc_nth,   NULL,          }, },
	{"length",                builtin_length,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"apply",                 builtin_apply,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"call/eh",               builtin_call_eh,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"fopen",                 builtin_fopen,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"read",                  builtin_read,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"parse",                 builtin_parse,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"stringify",             builtin_stringify,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"prin1",                 builtin_prin1,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"princ",                 builtin_princ,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"println",               builtin_println,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"print",                 builtin_print,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"fprint",                builtin_fprint,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"fprin1",                builtin_fprin1,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"newline",               builtin_newline,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"dlopen",                builtin_dlopen,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"dlsym",                 builtin_dlsym,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"argv",                  builtin_argv,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"go",                    builtin_go,                      {NULL,         NULL,          NULL,          NULL,          }, },
	{"chanl",                 builtin_chanl,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"chanl-num",             builtin_chanl_num,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"send",                  builtin_send,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"recv",                  builtin_recv,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"error",                 builtin_error,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"raise",                 builtin_raise,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"throw",                 builtin_throw,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"reverse",               builtin_reverse,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"fclose",                builtin_fclose,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"feof",                  builtin_feof,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"nthcdr",                builtin_nthcdr,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"tail",                  builtin_tail,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"char?",                 builtin_char_p,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"integer?",              builtin_integer_p,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"fix-int?",              builtin_fix_int_p,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"big-int?",              builtin_big_int_p,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"f64?",                  builtin_f64_p,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"pair?",                 builtin_pair_p,                  {NULL,         NULL,          NULL,          NULL,          }, },

	{"symbol?",               builtin_symbol_p,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"string->symbol",        builtin_string_to_symbol,        {NULL,         NULL,          NULL,          NULL,          }, },

	{"string?",               builtin_string_p,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"string",                builtin_string,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"substring",             builtin_substring,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"sub-string",            builtin_sub_string,              {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-string",           builtin_make_string,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"symbol->string",        builtin_symbol_to_string,        {NULL,         NULL,          NULL,          NULL,          }, },
	{"number->string",        builtin_number_to_string,        {NULL,         NULL,          NULL,          NULL,          }, },
	{"integer->string",       builtin_integer_to_string,       {NULL,         NULL,          NULL,          NULL,          }, },
	{"f64->string",           builtin_f64_to_string,           {NULL,         NULL,          NULL,          NULL,          }, },
	{"vector->string",        builtin_vector_to_string,        {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector->string",    builtin_bytevector_to_string,    {NULL,         NULL,          NULL,          NULL,          }, },
	{"list->string",          builtin_list_to_string,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"->string",              builtin_to_string,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"sref",                  builtin_sref,                    {NULL,         NULL,          inlfunc_sref,  NULL,          }, },
	{"set-sref!",             builtin_set_sref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"fill-string!",          builtin_fill_string,             {NULL,         NULL,          NULL,          NULL,          }, },

	{"error?",                builtin_error_p,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"procedure?",            builtin_procedure_p,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"proc?",                 builtin_proc_p,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"dlproc?",               builtin_dlproc_p,                {NULL,         NULL,          NULL,          NULL,          }, },

	{"vector?",               builtin_vector_p,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"vector",                builtin_vector,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-vector",           builtin_make_vector,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"subvector",             builtin_subvector,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"sub-vector",            builtin_sub_vector,              {NULL,         NULL,          NULL,          NULL,          }, },
	{"list->vector",          builtin_list_to_vector,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"string->vector",        builtin_string_to_vector,        {NULL,         NULL,          NULL,          NULL,          }, },
	{"vref",                  builtin_vref,                    {NULL,         NULL,          inlfunc_vref,  NULL,          }, },
	{"set-vref!",             builtin_set_vref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"cas-vref!",             builtin_cas_vref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"fill-vector!",          builtin_fill_vector,             {NULL,         NULL,          NULL,          NULL,          }, },

	{"list?",                 builtin_list_p,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"list",                  builtin_list,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"list*",                 builtin_list8,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-list",             builtin_make_list,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"vector->list",          builtin_vector_to_list,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"string->list",          builtin_string_to_list,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-nth!",              builtin_set_nth,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-nthcdr!",           builtin_set_nthcdr,              {NULL,         NULL,          NULL,          NULL,          }, },

	{"bytevector?",           builtin_bytevector_p,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector",            builtin_bytevector,              {NULL,         NULL,          NULL,          NULL,          }, },
	{"subbytevector",         builtin_subbytevector,           {NULL,         NULL,          NULL,          NULL,          }, },
	{"sub-bytevector",        builtin_sub_bytevector,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-bytevector",       builtin_make_bytevector,         {NULL,         NULL,          NULL,          NULL,          }, },
	{"string->bytevector",    builtin_string_to_bytevector,    {NULL,         NULL,          NULL,          NULL,          }, },
	{"vector->bytevector",    builtin_vector_to_bytevector,    {NULL,         NULL,          NULL,          NULL,          }, },
	{"list->bytevector",      builtin_list_to_bytevector,      {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector->s8-list",   builtin_bytevector_to_s8_list,   {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector->u8-list",   builtin_bytevector_to_u8_list,   {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector->s8-vector", builtin_bytevector_to_s8_vector, {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector->u8-vector", builtin_bytevector_to_u8_vector, {NULL,         NULL,          NULL,          NULL,          }, },

	{"bvs8ref",               builtin_bvs8ref,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvs16ref",              builtin_bvs16ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvs32ref",              builtin_bvs32ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvs64ref",              builtin_bvs64ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu8ref",               builtin_bvu8ref,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu16ref",              builtin_bvu16ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu32ref",              builtin_bvu32ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu64ref",              builtin_bvu64ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvf32ref",              builtin_bvf32ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvf64ref",              builtin_bvf64ref,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvs8ref!",          builtin_set_bvs8ref,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvs16ref!",         builtin_set_bvs16ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvs32ref!",         builtin_set_bvs32ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvs64ref!",         builtin_set_bvs64ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvu8ref!",          builtin_set_bvu8ref,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvu16ref!",         builtin_set_bvu16ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvu32ref!",         builtin_set_bvu32ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvu64ref!",         builtin_set_bvu64ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvf32ref!",         builtin_set_bvf32ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-bvf64ref!",         builtin_set_bvf64ref,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"fill-bytevector!",      builtin_fill_bytevector,         {NULL,         NULL,          NULL,          NULL,          }, },

	{"chanl?",                builtin_chanl_p,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"dll?",                  builtin_dll_p,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"getcwd",                builtin_getcwd,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"cd",                    builtin_cd,                      {NULL,         NULL,          NULL,          NULL,          }, },
	{"fgetc",                 builtin_fgetc,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"fgeti",                 builtin_fgeti,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"fwrite",                builtin_fwrite,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"fgets",                 builtin_fgets,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"fgetb",                 builtin_fgetb,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"fgetd",                 builtin_fgetd,                   {NULL,         NULL,          NULL,          NULL,          }, },

	{"set-car!",              builtin_set_car,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-cdr!",              builtin_set_cdr,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"box",                   builtin_box,                     {NULL,         inlfunc_box,   NULL,          NULL,          }, },
	{"unbox",                 builtin_unbox,                   {NULL,         inlfunc_unbox, NULL,          NULL,          }, },
	{"set-box!",              builtin_set_box,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"cas-box!",              builtin_cas_box,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"box?",                  builtin_box_p,                   {NULL,         NULL,          NULL,          NULL,          }, },

	{"number?",               builtin_number_p,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"string->number",        builtin_string_to_number,        {NULL,         NULL,          NULL,          NULL,          }, },
	{"char->integer",         builtin_char_to_integer,         {NULL,         NULL,          NULL,          NULL,          }, },
	{"symbol->integer",       builtin_symbol_to_integer,       {NULL,         NULL,          NULL,          NULL,          }, },
	{"integer->char",         builtin_integer_to_char,         {NULL,         NULL,          NULL,          NULL,          }, },
	{"number->f64",           builtin_number_to_f64,           {NULL,         NULL,          NULL,          NULL,          }, },
	{"number->integer",       builtin_number_to_integer,       {NULL,         NULL,          NULL,          NULL,          }, },

	{"map",                   builtin_map,                     {NULL,         NULL,          NULL,          NULL,          }, },
	{"foreach",               builtin_foreach,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"andmap",                builtin_andmap,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"ormap",                 builtin_ormap,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"memq",                  builtin_memq,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"member",                builtin_member,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"memp",                  builtin_memp,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"filter",                builtin_filter,                  {NULL,         NULL,          NULL,          NULL,          }, },

	{"sleep",                 builtin_sleep,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"srand",                 builtin_srand,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"rand",                  builtin_rand,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"get-time",              builtin_get_time,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"remove-file",           builtin_remove_file,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"time",                  builtin_time,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"system",                builtin_system,                  {NULL,         NULL,          NULL,          NULL,          }, },

	{"hash",                  builtin_hash,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"hash-num",              builtin_hash_num,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-hash",             builtin_make_hash,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"hasheqv",               builtin_hasheqv,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-hasheqv",          builtin_make_hasheqv,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"hashequal",             builtin_hashequal,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-hashequal",        builtin_make_hashequal,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"hash?",                 builtin_hash_p,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"hasheq?",               builtin_hasheq_p,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"hasheqv?",              builtin_hasheqv_p,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"hashequal?",            builtin_hashequal_p,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"href",                  builtin_href,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"href!",                 builtin_href1,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-href!",             builtin_set_href,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"set-href*!",            builtin_set_href8,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"hash-clear!",           builtin_hash_clear,              {NULL,         NULL,          NULL,          NULL,          }, },
	{"hash->list",            builtin_hash_to_list,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"hash-keys",             builtin_hash_keys,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"hash-values",           builtin_hash_values,             {NULL,         NULL,          NULL,          NULL,          }, },

	{"pmatch",                builtin_pmatch,                  {NULL,         NULL,          NULL,          NULL,          }, },

	{"odd?",                  builtin_odd_p,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"even?",                 builtin_even_p,                  {NULL,         NULL,          NULL,          NULL,          }, },

	{NULL,                    NULL,                            {NULL,         NULL,          NULL,          NULL,          }, },
};

static const size_t BuiltinSymbolNum=sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct)-1;

FklBuiltinInlineFunc fklGetBuiltInInlineFunc(uint32_t idx,uint32_t argNum)
{
	if(idx>=BuiltinSymbolNum)
		return NULL;
	return builtInSymbolList[idx].inlfunc[argNum];
}

uint8_t* fklGetBultinSymbolModifyMark(uint32_t* p)
{
	*p=BuiltinSymbolNum;
	uint8_t* r=(uint8_t*)calloc(BuiltinSymbolNum,sizeof(uint8_t));
	FKL_ASSERT(r);
	return r;
}

//void print_refs(FklHashTable* ht,FklSymbolTable* publicSymbolTable)
//{
//	uint32_t size=ht->size;
//	FklHashTableNode** base=ht->base;
//	for(uint32_t i=0;i<size;i++)
//	{
//		uint32_t c=0;
//		for(FklHashTableNode* h=base[i];h;h=h->next)
//			c++;
//		fprintf(stderr,"%4u:",c);
//		for(FklHashTableNode* h=base[i];h;h=h->next)
//		{
//			FklSymbolDef* def=(FklSymbolDef*)h->data;
//			fputc('|',stderr);
//			fklPrintString(fklGetSymbolWithId(def->k.id,publicSymbolTable)->symbol,stderr);
//			fputc('|',stderr);
//			fputs(" -> ",stderr);
//		}
//		fputs("(/)\n",stderr);
//	}
//}

void fklInitGlobCodegenEnv(FklCodegenEnv* curEnv,FklSymbolTable* publicSymbolTable)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddCodegenBuiltinRefBySid(fklAddSymbolCstr(list->s,publicSymbolTable)->id,curEnv);
}

void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* table)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddSymbolCstr(list->s,table);
}

inline static void init_vm_public_data(PublicBuiltInData* pd,FklVMgc* gc,FklSymbolTable* table)
{
	static const char* builtInHeadSymbolTableCstr[4]=
	{
		"quote",
		"qsquote",
		"unquote",
		"unqtesp",
	};
	FklVMvalue* builtInStdin=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stdin),gc);
	FklVMvalue* builtInStdout=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stdout),gc);
	FklVMvalue* builtInStderr=fklCreateVMvalueNoGC(FKL_TYPE_FP,fklCreateVMfp(stderr),gc);
	pd->sysIn=builtInStdin;
	pd->sysOut=builtInStdout;
	pd->sysErr=builtInStderr;
	pd->patterns=fklInitBuiltInStringPattern(table);
	for(int i=0;i<3;i++)
		pd->builtInHeadSymbolTable[i]=fklAddSymbolCstr(builtInHeadSymbolTableCstr[i],table)->id;
}

void fklInitGlobalVMclosure(FklVMframe* frame,FklVM* exe)
{
	FklVMCompoundFrameVarRef* f=&frame->u.c.lr;
	static const size_t RefCount=(sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct))-1;
	f->rcount=RefCount;
	FklVMvarRef** closure=(FklVMvarRef**)malloc(sizeof(FklVMvarRef*)*f->rcount);
	FKL_ASSERT(closure);
	f->ref=closure;
	FklVMudata* pud=fklCreateVMudata(0
				,&PublicBuiltInDataMethodTable
				,FKL_VM_NIL
				,sizeof(PublicBuiltInData));
	FKL_DECL_UD_DATA(pd,PublicBuiltInData,pud);
	init_vm_public_data(pd,exe->gc,exe->symbolTable);
	FklVMvalue* publicUserData=fklCreateVMvalueNoGC(FKL_TYPE_USERDATA
			,pud
			,exe->gc);

	closure[FKL_VM_STDIN_IDX]=fklCreateClosedVMvarRef(pd->sysIn);
	closure[FKL_VM_STDOUT_IDX]=fklCreateClosedVMvarRef(pd->sysOut);
	closure[FKL_VM_STDERR_IDX]=fklCreateClosedVMvarRef(pd->sysErr);

	FklSymbolTable* table=exe->symbolTable;
	for(size_t i=3;i<RefCount;i++)
	{
		FklVMdlproc* proc=fklCreateVMdlproc(builtInSymbolList[i].f,NULL,publicUserData);
		proc->sid=fklAddSymbolCstr(builtInSymbolList[i].s,table)->id;
		FklVMvalue* v=fklCreateVMvalueNoGC(FKL_TYPE_DLPROC,proc,exe->gc);
		closure[i]=fklCreateClosedVMvarRef(v);
	}
}

void fklInitGlobalVMclosureForProc(FklVMproc* proc,FklVM* exe)
{
	static const size_t RefCount=(sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct))-1;
	proc->count=RefCount;
	FklVMvarRef** closure=(FklVMvarRef**)malloc(sizeof(FklVMvarRef*)*proc->count);
	FKL_ASSERT(closure);
	proc->closure=closure;
	FklVMudata* pud=fklCreateVMudata(0
				,&PublicBuiltInDataMethodTable
				,FKL_VM_NIL
				,sizeof(PublicBuiltInData));
	FKL_DECL_UD_DATA(pd,PublicBuiltInData,pud);
	init_vm_public_data(pd,exe->gc,exe->symbolTable);
	FklVMvalue* publicUserData=fklCreateVMvalueNoGC(FKL_TYPE_USERDATA
			,pud
			,exe->gc);

	closure[FKL_VM_STDIN_IDX]=fklCreateClosedVMvarRef(pd->sysIn);
	closure[FKL_VM_STDOUT_IDX]=fklCreateClosedVMvarRef(pd->sysOut);
	closure[FKL_VM_STDERR_IDX]=fklCreateClosedVMvarRef(pd->sysErr);

	FklSymbolTable* table=exe->symbolTable;
	for(size_t i=3;i<RefCount;i++)
	{
		FklVMdlproc* proc=fklCreateVMdlproc(builtInSymbolList[i].f,NULL,publicUserData);
		proc->sid=fklAddSymbolCstr(builtInSymbolList[i].s,table)->id;
		FklVMvalue* v=fklCreateVMvalueNoGC(FKL_TYPE_DLPROC,proc,exe->gc);
		closure[i]=fklCreateClosedVMvarRef(v);
	}
}

