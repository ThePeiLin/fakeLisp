#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/grammer.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/base.h>
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

#define DECL_AND_CHECK_ARG(a,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	if(!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_CHECK_ARG2(a,b,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	FklVMvalue* b=FKL_VM_POP_ARG(exe);\
	if(!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_CHECK_ARG3(a,b,c,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	FklVMvalue* b=FKL_VM_POP_ARG(exe);\
	FklVMvalue* c=FKL_VM_POP_ARG(exe);\
	if(!c||!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_SET_DEFAULT(a,v) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	if(!a)\
		a=v;

typedef struct
{
	FklVMvalue* sysIn;
	FklVMvalue* sysOut;
	FklVMvalue* sysErr;
	FklSid_t seek_cur;
	FklSid_t seek_set;
	FklSid_t seek_end;
}PublicBuiltInData;

static void _public_builtin_userdata_atomic(const FklVMudata* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(d,PublicBuiltInData,ud);
	fklVMgcToGray(d->sysIn,gc);
	fklVMgcToGray(d->sysOut,gc);
	fklVMgcToGray(d->sysErr,gc);
}

static FklVMudMetaTable PublicBuiltInDataMetaTable=
{
	.size=sizeof(PublicBuiltInData),
	.__princ=NULL,
	.__prin1=NULL,
	.__finalizer=NULL,
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
		"unexpected-eof",
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
		"unsupported-operation",
		"import-missing",
		"export-outer-ref-prod-group",
		"import-reader-macro-error",
		"analysis-table-genrate-failed",
		"regex-compile-failed",
		"grammer-create-failed",
	};

	for(size_t i=0;i<FKL_BUILTIN_ERR_NUM;i++)
		errorTypeId[i]=fklAddSymbolCstr(builtInErrorType[i],table)->id;
}

FklSid_t fklGetBuiltinErrorType(FklBuiltinErrorType type,FklSid_t errorTypeId[FKL_ERR_INCORRECT_TYPE_VALUE])
{
	return errorTypeId[type];
}

//builtin functions

static int builtin_car(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.car";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(obj));
	return 0;
}

static int builtin_car_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.car-set!";
	DECL_AND_CHECK_ARG2(obj,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	FKL_VM_CAR(obj)=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_cdr(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.cdr";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_CDR(obj));
	return 0;
}

static int builtin_cdr_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.cdr-set!";
	DECL_AND_CHECK_ARG2(obj,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	FKL_VM_CDR(obj)=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_cons(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.cons";
	DECL_AND_CHECK_ARG2(car,cdr,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,car,cdr));
	return 0;
}

static int __fkl_str_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_STR(cur))
		return 1;
	fklStringCat(&FKL_VM_STR(retval),FKL_VM_STR(cur));
	return 0;
}

static int __fkl_vector_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_VECTOR(cur))
		return 1;
	fklVMvecConcat(FKL_VM_VEC(retval),FKL_VM_VEC(cur));
	return 0;
}

static int __fkl_bytevector_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_BYTEVECTOR(cur))
		return 1;
	fklBytevectorCat(&FKL_VM_BVEC(retval),FKL_VM_BVEC(cur));
	return 0;
}

static int __fkl_userdata_append(FklVMvalue* retval,FklVMvalue* cur)
{
	if(!FKL_IS_USERDATA(cur)||FKL_VM_UD(retval)->t!=FKL_VM_UD(cur)->t)
		return 1;
	return fklAppendVMudata(FKL_VM_UD(retval),FKL_VM_UD(cur));
}

static int (*const valueAppend[FKL_VM_VALUE_GC_TYPE_NUM])(FklVMvalue* retval,FklVMvalue* cur)=
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

static int builtin_copy(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.copy";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* retval=fklCopyVMvalue(obj,exe);
	if(!retval)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static inline FklVMvalue* get_fast_value(FklVMvalue* head)
{
	return FKL_IS_PAIR(head)
		&&FKL_IS_PAIR(FKL_VM_CDR(head))
		&&FKL_IS_PAIR(FKL_VM_CDR(FKL_VM_CDR(head)))?FKL_VM_CDR(FKL_VM_CDR(head)):FKL_VM_NIL;
}

static inline FklVMvalue* get_initial_fast_value(FklVMvalue* pr)
{
	return FKL_IS_PAIR(pr)?FKL_VM_CDR(pr):FKL_VM_NIL;
}

static inline FklVMvalue** copy_list(FklVMvalue** pv,FklVM* exe)
{
	FklVMvalue* v=*pv;
	for(;FKL_IS_PAIR(v);v=FKL_VM_CDR(v),pv=&FKL_VM_CDR(*pv))
		*pv=fklCreateVMvaluePair(exe,FKL_VM_CAR(v),FKL_VM_CDR(v));
	return pv;
}

static int builtin_append(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.append";
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=FKL_VM_POP_ARG(exe);
		if(cur)
			retval=fklCopyVMvalue(retval,exe);
		if(!retval)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		for(;cur;cur=FKL_VM_POP_ARG(exe))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	else
	{
		FklVMvalue** prev=&retval;
		for(;cur;cur=FKL_VM_POP_ARG(exe))
		{
			FklVMvalue* pr=*prev;
			if(fklIsList(pr)
					&&(prev=copy_list(prev,exe),*prev==FKL_VM_NIL))
				*prev=cur;
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_append1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.append!";
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=FKL_VM_POP_ARG(exe);
		for(;cur;cur=FKL_VM_POP_ARG(exe))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	else
	{
		FklVMvalue** prev=&retval;
		for(;cur;cur=FKL_VM_POP_ARG(exe))
		{
			for(FklVMvalue* head=get_initial_fast_value(*prev)
					;FKL_IS_PAIR(*prev)
					;prev=&FKL_VM_CDR(*prev)
					,head=get_fast_value(head))
				if(head==*prev)
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_CIR_REF,exe);
			if(*prev==FKL_VM_NIL)
				*prev=cur;
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_eq(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.eq";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe
			,fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_eqv(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.eqv";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe
			,(fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_equal(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.equal";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe
			,(fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_add(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.+";
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	for(;cur;cur=FKL_VM_POP_ARG(exe))
		if(fklProcessVMnumAdd(cur,&r64,&rd,&bi))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumAddResult(exe,r64,rd,&bi));
	return 0;
}

static int builtin_add_1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.1+";
	DECL_AND_CHECK_ARG(arg,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* r=fklProcessVMnumInc(exe,arg);
	if(r)
		FKL_VM_PUSH_VALUE(exe,r);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_sub(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.-";
	DECL_AND_CHECK_ARG(prev,Pname);
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	double rd=0.0;
	int64_t r64=0;
	FKL_CHECK_TYPE(prev,fklIsVMnumber,Pname,exe);
	if(!cur)
	{
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,fklProcessVMnumNeg(exe,prev));
	}
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt0(&bi);
		for(;cur;cur=FKL_VM_POP_ARG(exe))
			if(fklProcessVMnumAdd(cur,&r64,&rd,&bi))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,fklProcessVMnumSubResult(exe,prev,r64,rd,&bi));
	}
	return 0;
}

static int builtin_abs(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.abs";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_F64(obj))
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,fabs(FKL_VM_F64(obj))));
	else
	{
		if(FKL_IS_FIX(obj))
		{
			int64_t i=llabs(FKL_GET_FIX(obj));
			if(i>FKL_FIX_INT_MAX)
				FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBigIntWithI64(exe,i));
			else
				FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(i));
		}
		else
		{
			FklVMvalue* r=fklCreateVMvalueBigInt(exe,FKL_VM_BI(obj));
			FKL_VM_BI(r)->neg=0;
			FKL_VM_PUSH_VALUE(exe,r);
		}
	}
	return 0;
}

static int builtin_sub_1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.-1+";
	DECL_AND_CHECK_ARG(arg,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* r=fklProcessVMnumDec(exe,arg);
	if(r)
		FKL_VM_PUSH_VALUE(exe,r);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_mul(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.*";
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=FKL_VM_POP_ARG(exe))
		if(fklProcessVMnumMul(cur,&r64,&rd,&bi))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumMulResult(exe,r64,rd,&bi));
	return 0;
}

static int builtin_idiv(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.//";
	DECL_AND_CHECK_ARG2(prev,cur,Pname);
	int64_t r64=1;
	FKL_CHECK_TYPE(prev,fklIsVMint,Pname,exe);
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=FKL_VM_POP_ARG(exe))
		if(fklProcessVMintMul(cur,&r64,&bi))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(r64==0||FKL_IS_0_BIG_INT(&bi))
	{
		fklUninitBigInt(&bi);
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumIdivResult(exe,prev,r64,&bi));
	return 0;
}

static int builtin_div(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin./";
	DECL_AND_CHECK_ARG(prev,Pname);
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	int64_t r64=1;
	double rd=1.0;
	FKL_CHECK_TYPE(prev,fklIsVMnumber,Pname,exe);
	if(!cur)
	{
		fklResBp(exe);
		FklVMvalue* r=fklProcessVMnumRec(exe,prev);
		if(!r)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt1(&bi);
		for(;cur;cur=FKL_VM_POP_ARG(exe))
			if(fklProcessVMnumMul(cur,&r64,&rd,&bi))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		if(r64==0||FKL_IS_0_BIG_INT(&bi)||!islessgreater(rd,0.0))
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
		}
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,fklProcessVMnumDivResult(exe,prev,r64,rd,&bi));
	}
	return 0;
}

static int builtin_mod(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.%";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(fir,fklIsVMnumber,Pname,exe);
	FKL_CHECK_TYPE(sec,fklIsVMnumber,Pname,exe);
	FklVMvalue* r=fklProcessVMnumMod(exe,fir,sec);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_eqn(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.=";
	int r=1;
	int err=0;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)==0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=FKL_VM_POP_ARG(exe));
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_gt(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.>";
	int r=1;
	int err=0;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)>0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=FKL_VM_POP_ARG(exe));
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_ge(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.>=";
	int err=0;
	int r=1;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)>=0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=FKL_VM_POP_ARG(exe));
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_lt(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.<";
	int err=0;
	int r=1;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)<0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=FKL_VM_POP_ARG(exe));
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_le(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.<=";
	int err=0;
	int r=1;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)<=0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=FKL_VM_POP_ARG(exe));
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_char_to_integer(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.char->integer";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_CHR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(FKL_GET_CHR(obj)));
	return 0;
}

static int builtin_integer_to_char(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.integer->char";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsVMint,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_CHR(fklGetInt(obj)));
	return 0;
}

static int builtin_list_to_vector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.list->vector";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsList,Pname,exe);
	size_t len=fklVMlistLength(obj);
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	FklVMvec* vec=FKL_VM_VEC(r);
	for(size_t i=0;obj!=FKL_VM_NIL;i++,obj=FKL_VM_CDR(obj))
		vec->base[i]=FKL_VM_CAR(obj);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_vector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string->vector";
	DECL_AND_CHECK_ARG(obj,Pname)
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FklString* str=FKL_VM_STR(obj);
	size_t len=str->size;
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	FklVMvec* vec=FKL_VM_VEC(r);
	for(size_t i=0;i<len;i++)
		vec->base[i]=FKL_MAKE_VM_CHR(str->str[i]);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-list";
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsVMint,Pname,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	FklVMvalue* t=content?content:FKL_VM_NIL;
	for(size_t i=0;i<len;i++)
	{
		*cur=fklCreateVMvaluePairWithCar(exe,t);
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string->list";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklString* str=FKL_VM_STR(obj);
	FklVMvalue** cur=&r;
	for(size_t i=0;i<str->size;i++)
	{
		*cur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_CHR(str->str[i]));
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_bytevector_to_s8_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->s8-list";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	int8_t* s8a=(int8_t*)bvec->ptr;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<size;i++)
	{
		*cur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_FIX(s8a[i]));
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_bytevector_to_u8_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->s8-list";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	uint8_t* u8a=bvec->ptr;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<size;i++)
	{
		*cur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_FIX(u8a[i]));
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_bytevector_to_s8_vector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->s8-vector";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	int8_t* s8a=(int8_t*)bvec->ptr;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(size_t i=0;i<size;i++)
		v->base[i]=FKL_MAKE_VM_FIX(s8a[i]);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_bytevector_to_u8_vector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->u8-vector";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	uint8_t* u8a=bvec->ptr;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(size_t i=0;i<size;i++)
		v->base[i]=FKL_MAKE_VM_FIX(u8a[i]);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_vector_to_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vector->list";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* vec=FKL_VM_VEC(obj);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<vec->size;i++)
	{
		*cur=fklCreateVMvaluePairWithCar(exe,vec->base[i]);
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string";
	size_t size=exe->tp-exe->bp;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	size_t i=0;
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur!=NULL
			;cur=FKL_VM_POP_ARG(exe),i++)
	{
		FKL_CHECK_TYPE(cur,FKL_IS_CHR,Pname,exe);
		str->str[i]=FKL_GET_CHR(cur);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-string";
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsVMint,Pname,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(len,NULL));
	FklString* str=FKL_VM_STR(r);
	char ch=0;
	if(content)
	{
		FKL_CHECK_TYPE(content,FKL_IS_CHR,Pname,exe);
		ch=FKL_GET_CHR(content);
	}
	memset(str->str,ch,len);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_vector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-vector";
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsVMint,Pname,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	if(content)
	{
		FklVMvec* vec=FKL_VM_VEC(r);
		for(size_t i=0;i<len;i++)
			vec->base[i]=content;
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_substring(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.substring";
	DECL_AND_CHECK_ARG3(ostr,vstart,vend,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ostr,FKL_IS_STR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,Pname,exe);
	FklString* str=FKL_VM_STR(ostr);
	size_t size=str->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,str->str+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.sub-string";
	DECL_AND_CHECK_ARG3(ostr,vstart,vsize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ostr,FKL_IS_STR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,Pname,exe);
	FklString* str=FKL_VM_STR(ostr);
	size_t size=str->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,str->str+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_subbytevector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.subbytevector";
	DECL_AND_CHECK_ARG3(ostr,vstart,vend,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(ostr);
	size_t size=bvec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(size,bvec->ptr+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_bytevector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.sub-bytevector";
	DECL_AND_CHECK_ARG3(ostr,vstart,vsize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(ostr);
	size_t size=bvec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(size,bvec->ptr+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_subvector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.subvector";
	DECL_AND_CHECK_ARG3(ovec,vstart,vend,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ovec,FKL_IS_VECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,Pname,exe);
	FklVMvec* vec=FKL_VM_VEC(ovec);
	size_t size=vec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vend)
			||start>size
			||end<start
			||end>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueVecWithPtr(exe,size,vec->base+start);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_vector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.sub-vector";
	DECL_AND_CHECK_ARG3(ovec,vstart,vsize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ovec,FKL_IS_VECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,Pname,exe);
	FklVMvec* vec=FKL_VM_VEC(ovec);
	size_t size=vec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(fklVMnumberLt0(vstart)
			||fklVMnumberLt0(vsize)
			||start>size
			||start+osize>size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueVecWithPtr(exe,size,vec->base+start);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* retval=FKL_VM_NIL;
	if(FKL_IS_SYM(obj))
		retval=fklCreateVMvalueStr(exe,fklCopyString(fklGetSymbolWithId(FKL_GET_SYM(obj),exe->symbolTable)->symbol));
	else if(FKL_IS_STR(obj))
		retval=fklCreateVMvalueStr(exe,fklCopyString(FKL_VM_STR(obj)));
	else if(FKL_IS_CHR(obj))
	{
		FklString* r=fklCreateString(1,NULL);
		r->str[0]=FKL_GET_CHR(obj);
		retval=fklCreateVMvalueStr(exe,r);
	}
	else if(fklIsVMnumber(obj))
	{
		retval=fklCreateVMvalueStr(exe,NULL);
		if(fklIsVMint(obj))
		{
			if(FKL_IS_BIG_INT(obj))
				FKL_VM_STR(retval)=fklBigIntToString(FKL_VM_BI(obj),10);
			else
			{
				FklBigInt bi=FKL_BIG_INT_INIT;
				uint8_t t[64]={0};
				bi.size=64;
				bi.digits=t;
				fklSetBigIntI(&bi,fklGetInt(obj));
				FKL_VM_STR(retval)=fklBigIntToString(&bi,10);
			}
		}
		else
		{
			char buf[64]={0};
			size_t size=snprintf(buf,64,"%lf",FKL_VM_F64(obj));
			FKL_VM_STR(retval)=fklCreateString(size,buf);
		}
	}
	else if(FKL_IS_BYTEVECTOR(obj))
	{
		FklBytevector* bvec=FKL_VM_BVEC(obj);
		retval=fklCreateVMvalueStr(exe,fklCreateString(bvec->size,(char*)bvec->ptr));
	}
	else if(FKL_IS_VECTOR(obj))
	{
		FklVMvec* vec=FKL_VM_VEC(obj);
		size_t size=vec->size;
		retval=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
		FklString* str=FKL_VM_STR(retval);
		for(size_t i=0;i<size;i++)
		{
			FKL_CHECK_TYPE(vec->base[i],FKL_IS_CHR,Pname,exe);
			str->str[i]=FKL_GET_CHR(vec->base[i]);
		}
	}
	else if(fklIsList(obj))
	{
		size_t size=fklVMlistLength(obj);
		retval=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
		FklString* str=FKL_VM_STR(retval);
		for(size_t i=0;i<size;i++)
		{
			FKL_CHECK_TYPE(FKL_VM_CAR(obj),FKL_IS_CHR,Pname,exe);
			str->str[i]=FKL_GET_CHR(FKL_VM_CAR(obj));
			obj=FKL_VM_CDR(obj);
		}
	}
	else if(FKL_IS_USERDATA(obj)&&fklIsAbleToStringUd(FKL_VM_UD(obj)))
		retval=fklCreateVMvalueStr(exe,fklUdToString(FKL_VM_UD(obj)));
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_symbol_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.symbol->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_SYM,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe
			,fklCopyString(fklGetSymbolWithId(FKL_GET_SYM(obj),exe->symbolTable)->symbol));
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_string_to_symbol(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string->symbol";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(fklAddSymbol(FKL_VM_STR(obj),exe->symbolTable)->id));
	return 0;
}

static int builtin_symbol_to_integer(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.symbol->integer";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_SYM,Pname,exe);
	FklVMvalue* r=fklMakeVMint(FKL_GET_SYM(obj),exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_number(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string->number";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklString* str=FKL_VM_STR(obj);
	if(fklIsNumberString(str))
	{
		if(fklIsFloat(str->str,str->size))
			r=fklCreateVMvalueF64(exe,fklStringToDouble(str));
		else
		{
			int base=0;
			int64_t i=fklStringToInt(str->str,str->size,&base);
			if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
				r=fklCreateVMvalueBigIntWithString(exe,str,base);
			else
				r=FKL_MAKE_VM_FIX(i);
		}
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_number_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.number->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* radix=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	if(fklIsVMint(obj))
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_CHECK_TYPE(radix,fklIsVMint,Pname,exe);
			int64_t t=fklGetInt(radix);
			if(t!=8&&t!=10&&t!=16)
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDRADIX,exe);
			base=t;
		}
		if(FKL_IS_BIG_INT(obj))
			FKL_VM_STR(retval)=fklBigIntToString(FKL_VM_BI(obj),base);
		else
		{
			FklBigInt bi=FKL_BIG_INT_INIT;
			uint8_t t[64]={0};
			bi.size=64;
			bi.digits=t;
			fklSetBigIntI(&bi,fklGetInt(obj));
			FKL_VM_STR(retval)=fklBigIntToString(&bi,base);
		}
	}
	else
	{
		char buf[64]={0};
		if(radix)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOMANYARG,exe);
		size_t size=snprintf(buf,64,"%lf",FKL_VM_F64(obj));
		FKL_VM_STR(retval)=fklCreateString(size,buf);
	}
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_integer_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.integer->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* radix=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsVMint,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	uint32_t base=10;
	if(radix)
	{
		FKL_CHECK_TYPE(radix,fklIsVMint,Pname,exe);
		int64_t t=fklGetInt(radix);
		if(t!=8&&t!=10&&t!=16)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDRADIX,exe);
		base=t;
	}
	if(FKL_IS_BIG_INT(obj))
		FKL_VM_STR(retval)=fklBigIntToString(FKL_VM_BI(obj),base);
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		uint8_t t[64]={0};
		bi.size=64;
		bi.digits=t;
		fklSetBigIntI(&bi,fklGetInt(obj));
		FKL_VM_STR(retval)=fklBigIntToString(&bi,base);
	}
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_f64_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.f64->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,FKL_IS_F64,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	char buf[64]={0};
	size_t size=snprintf(buf,64,"%lf",FKL_VM_F64(obj));
	FKL_VM_STR(retval)=fklCreateString(size,buf);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_vector_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vector->string";
	DECL_AND_CHECK_ARG(vec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	for(size_t i=0;i<size;i++)
	{
		FKL_CHECK_TYPE(v->base[i],FKL_IS_CHR,Pname,exe);
		str->str[i]=FKL_GET_CHR(v->base[i]);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_bytevector_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->string";
	DECL_AND_CHECK_ARG(vec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(vec,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(vec);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(bvec->size,(char*)bvec->ptr));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_bytevector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string->bytevector";
	DECL_AND_CHECK_ARG(str,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(str,FKL_IS_STR,Pname,exe);
	FklString* s=FKL_VM_STR(str);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(s->size,(uint8_t*)s->str));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_vector_to_bytevector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vector->bytevector";
	DECL_AND_CHECK_ARG(vec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(v->size,NULL));
	uint64_t size=v->size;
	FklVMvalue** base=v->base;
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(uint64_t i=0;i<size;i++)
	{
		FklVMvalue* cur=base[i];
		FKL_CHECK_TYPE(cur,fklIsVMint,Pname,exe);
		ptr[i]=fklGetInt(cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_list_to_bytevector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.list->bytevector";
	DECL_AND_CHECK_ARG(list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(fklVMlistLength(list),NULL));
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(size_t i=0;list!=FKL_VM_NIL;i++,list=FKL_VM_CDR(list))
	{
		FklVMvalue* cur=FKL_VM_CAR(list);
		FKL_CHECK_TYPE(cur,fklIsVMint,Pname,exe);
		ptr[i]=fklGetInt(cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_list_to_string(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.list->string";
	DECL_AND_CHECK_ARG(list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	size_t size=fklVMlistLength(list);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	for(size_t i=0;i<size;i++)
	{
		FKL_CHECK_TYPE(FKL_VM_CAR(list),FKL_IS_CHR,Pname,exe);
		str->str[i]=FKL_GET_CHR(FKL_VM_CAR(list));
		list=FKL_VM_CDR(list);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_number_to_f64(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.integer->f64";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* retval=fklCreateVMvalueF64(exe,0.0);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_FIX(obj))
		FKL_VM_F64(retval)=(double)FKL_GET_FIX(obj);
	else if(FKL_IS_BIG_INT(obj))
		FKL_VM_F64(retval)=fklBigIntToDouble(FKL_VM_BI(obj));
	else
		FKL_VM_F64(retval)=FKL_VM_F64(obj);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

#include<math.h>

static int builtin_number_to_integer(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.number->integer";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_F64(obj))
	{
		double d=FKL_VM_F64(obj);
		if(isgreater(d,(double)FKL_FIX_INT_MAX)||isless(d,FKL_FIX_INT_MIN))
			FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBigIntWithF64(exe,d));
		else
			FKL_VM_PUSH_VALUE(exe,fklMakeVMintD(d,exe));
	}
	else if(FKL_IS_BIG_INT(obj))
	{
		FklBigInt* bigint=FKL_VM_BI(obj);
		if(fklIsGtLtFixBigInt(bigint))
		{
			FklBigInt bi=FKL_BIG_INT_INIT;
			fklSetBigInt(&bi,bigint);
			FklVMvalue* r=fklCreateVMvalueBigInt(exe,NULL);
			*FKL_VM_BI(r)=bi;
			FKL_VM_PUSH_VALUE(exe,r);
		}
		else
			FKL_VM_PUSH_VALUE(exe,fklMakeVMint(fklBigIntToI64(bigint),exe));
	}
	else
		FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_nth(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.nth";
	DECL_AND_CHECK_ARG2(place,objlist,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(place,fklIsVMint,Pname,exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(uint64_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
			FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(objPair));
		else
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_nth_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.nth-set!";
	DECL_AND_CHECK_ARG3(place,objlist,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(place,fklIsVMint,Pname,exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(size_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			FKL_VM_CAR(objPair)=target;
			FKL_VM_PUSH_VALUE(exe,target);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDASSIGN,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_str_ref(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.str-ref";
	DECL_AND_CHECK_ARG2(str,place,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!fklIsVMint(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklString* s=FKL_VM_STR(str);
	size_t size=s->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_CHR(s->str[index]));
	return 0;
}

#define BV_U_S_8_REF(TYPE,WHERE) static const char Pname[]=(WHERE);\
	DECL_AND_CHECK_ARG2(bvec,place,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	r=bv->ptr[index];\
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(r));\
	return 0;

#define BV_REF(TYPE,WHERE,MAKER) static const char Pname[]=(WHERE);\
	DECL_AND_CHECK_ARG2(bvec,place,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&r,&bv->ptr[index],sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,MAKER(r,exe));\
	return 0;

#define BV_S_REF(TYPE,WHERE) BV_REF(TYPE,WHERE,fklMakeVMint)
#define BV_U_REF(TYPE,WHERE) BV_REF(TYPE,WHERE,fklMakeVMuint)

static int builtin_bvs8ref(FKL_CPROC_ARGL) {BV_U_S_8_REF(int8_t,"builtin.bvs8ref")}
static int builtin_bvs16ref(FKL_CPROC_ARGL) {BV_S_REF(int16_t,"builtin.bvs16ref")}
static int builtin_bvs32ref(FKL_CPROC_ARGL) {BV_S_REF(int32_t,"builtin.bvs32ref")}
static int builtin_bvs64ref(FKL_CPROC_ARGL) {BV_S_REF(int64_t,"builtin.bvs64ref")}

static int builtin_bvu8ref(FKL_CPROC_ARGL) {BV_U_S_8_REF(uint8_t,"builtin.bvu8ref")}
static int builtin_bvu16ref(FKL_CPROC_ARGL) {BV_U_REF(uint16_t,"builtin.bvu16ref")}
static int builtin_bvu32ref(FKL_CPROC_ARGL) {BV_U_REF(uint32_t,"builtin.bvu32ref")}
static int builtin_bvu64ref(FKL_CPROC_ARGL) {BV_U_REF(uint64_t,"builtin.bvu64ref")}

#undef BV_REF
#undef BV_S_REF
#undef BV_U_REF
#undef BV_U_S_8_REF

#define BV_F_REF(TYPE,WHERE) static const char Pname[]=(WHERE);\
	DECL_AND_CHECK_ARG2(bvec,place,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&r,&bv->ptr[index],sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,r));\
	return 0;

static int builtin_bvf32ref(FKL_CPROC_ARGL) {BV_F_REF(float,"builtin.bvf32ref")}
static int builtin_bvf64ref(FKL_CPROC_ARGL) {BV_F_REF(double,"builtin.bvf32ref")}
#undef BV_F_REF

#define SET_BV_S_U_8_REF(TYPE,WHERE) static const char Pname[]=(WHERE);\
	DECL_AND_CHECK_ARG3(bvec,place,target,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsVMint(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	bv->ptr[index]=r;\
	FKL_VM_PUSH_VALUE(exe,target);\
	return 0;

#define SET_BV_REF(TYPE,WHERE) static const char Pname[]=(WHERE);\
	DECL_AND_CHECK_ARG3(bvec,place,target,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsVMint(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&bv->ptr[index],&r,sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,target);\
	return 0;

static int builtin_bvs8set1(FKL_CPROC_ARGL) {SET_BV_S_U_8_REF(int8_t,"builtin.bvs8set!")}
static int builtin_bvs16set1(FKL_CPROC_ARGL) {SET_BV_REF(int16_t,"builtin.bvs16set!")}
static int builtin_bvs32set1(FKL_CPROC_ARGL) {SET_BV_REF(int32_t,"builtin.bvs32set!")}
static int builtin_bvs64set1(FKL_CPROC_ARGL) {SET_BV_REF(int64_t,"builtin.bvs64set!")}

static int builtin_bvu8set1(FKL_CPROC_ARGL) {SET_BV_S_U_8_REF(uint8_t,"builtin.bvu8set!")}
static int builtin_bvu16set1(FKL_CPROC_ARGL) {SET_BV_REF(uint16_t,"builtin.bvu16set!")}
static int builtin_bvu32set1(FKL_CPROC_ARGL) {SET_BV_REF(uint32_t,"builtin.bvu32set!")}
static int builtin_bvu64set1(FKL_CPROC_ARGL) {SET_BV_REF(uint64_t,"builtin.bvu64set!")}

#undef SET_BV_S_U_8_REF
#undef SET_BV_REF

#define SET_BV_F_REF(TYPE,WHERE) static const char Pname[]=(WHERE);\
	DECL_AND_CHECK_ARG3(bvec,place,target,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec)||!FKL_IS_F64(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=FKL_VM_F64(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&bv->ptr[index],&r,sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,target);\
	return 0;

static int builtin_bvf32set1(FKL_CPROC_ARGL) {SET_BV_F_REF(float,"builtin.bvf32set!")}
static int builtin_bvf64set1(FKL_CPROC_ARGL) {SET_BV_F_REF(double,"builtin.bvf64set!")}
#undef SET_BV_F_REF

static int builtin_str_set1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.str-set!";
	DECL_AND_CHECK_ARG3(str,place,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!fklIsVMint(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklString* s=FKL_VM_STR(str);
	size_t size=s->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(!FKL_IS_CHR(target)&&!fklIsVMint(target))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	s->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_string_fill(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.string-fill!";
	DECL_AND_CHECK_ARG2(str,content,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_CHR(content)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* s=FKL_VM_STR(str);
	memset(s->str,FKL_GET_CHR(content),s->size);
	FKL_VM_PUSH_VALUE(exe,str);
	return 0;
}

static int builtin_bytevector_fill(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.bytevector-fill!";
	DECL_AND_CHECK_ARG2(bvec,content,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!fklIsVMint(content)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklBytevector* bv=FKL_VM_BVEC(bvec);
	memset(bv->ptr,fklGetInt(content),bv->size);
	FKL_VM_PUSH_VALUE(exe,bvec);
	return 0;
}

static int builtin_vec_ref(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vec-ref";
	DECL_AND_CHECK_ARG2(vec,place,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[index]);
	return 0;
}

static int builtin_vec_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vec-set!";
	DECL_AND_CHECK_ARG3(vec,place,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	v->base[index]=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_vector_fill(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vector-fill!";
	DECL_AND_CHECK_ARG2(vec,content,Pname)
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	for(size_t i=0;i<size;i++)
		v->base[i]=content;
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_vec_cas(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.vec-cas!";
	FklVMvalue* vec=FKL_VM_POP_ARG(exe);
	FklVMvalue* place=FKL_VM_POP_ARG(exe);
	FklVMvalue* old=FKL_VM_POP_ARG(exe);
	FklVMvalue* new=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!place||!vec||!old||!new)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(v->base[index]==old)
	{
		v->base[index]=new;
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_nthcdr(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.nthcdr";
	DECL_AND_CHECK_ARG2(place,objlist,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(place,fklIsVMint,Pname,exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(size_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
			FKL_VM_PUSH_VALUE(exe,FKL_VM_CDR(objPair));
		else
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_tail(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.tail";
	DECL_AND_CHECK_ARG(objlist,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(;FKL_IS_PAIR(objPair)&&FKL_VM_CDR(objPair)!=FKL_VM_NIL;objPair=FKL_VM_CDR(objPair));
		FKL_VM_PUSH_VALUE(exe,objPair);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_nthcdr_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.nthcdr-set!";
	DECL_AND_CHECK_ARG3(place,objlist,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(place,fklIsVMint,Pname,exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(size_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			FKL_VM_CDR(objPair)=target;
			FKL_VM_PUSH_VALUE(exe,target);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_length(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.length";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	size_t len=0;
	if((obj==FKL_VM_NIL||FKL_IS_PAIR(obj))&&fklIsList(obj))
		len=fklVMlistLength(obj);
	else if(FKL_IS_STR(obj))
		len=FKL_VM_STR(obj)->size;
	else if(FKL_IS_VECTOR(obj))
		len=FKL_VM_VEC(obj)->size;
	else if(FKL_IS_BYTEVECTOR(obj))
		len=FKL_VM_BVEC(obj)->size;
	else if(FKL_IS_USERDATA(obj)&&fklUdHasLength(FKL_VM_UD(obj)))
		len=fklLengthVMudata(FKL_VM_UD(obj));
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_fopen(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fopen";
	DECL_AND_CHECK_ARG(filename,Pname);
	FklVMvalue* mode=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_STR(filename)||(mode&&!FKL_IS_STR(mode)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* filenameStr=FKL_VM_STR(filename);
	const char* modeStr=mode?FKL_VM_STR(mode)->str:"r";
	FILE* fp=fopen(filenameStr->str,modeStr);
	FklVMvalue* obj=NULL;
	if(!fp)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,filenameStr->str,0,FKL_ERR_FILEFAILURE,exe);
	else
		obj=fklCreateVMvalueFp(exe,fp,fklGetVMfpRwFromCstr(modeStr));
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_freopen(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.freopen";
	DECL_AND_CHECK_ARG2(stream,filename,Pname);
	FklVMvalue* mode=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_FP(stream)||!FKL_IS_STR(filename)||(mode&&!FKL_IS_STR(mode)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMfp* vfp=FKL_VM_FP(stream);
	FklString* filenameStr=FKL_VM_STR(filename);
	const char* modeStr=mode?FKL_VM_STR(mode)->str:"r";
	FILE* fp=freopen(filenameStr->str,modeStr,vfp->fp);
	if(!fp)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,filenameStr->str,0,FKL_ERR_FILEFAILURE,exe);
	else
	{
		vfp->fp=fp;
		vfp->rw=fklGetVMfpRwFromCstr(modeStr);
	}
	FKL_VM_PUSH_VALUE(exe,stream);
	return 0;
}

static int builtin_fclose(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fclose";
	DECL_AND_CHECK_ARG(vfp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(vfp,FKL_IS_FP,Pname,exe);
	FklVMfp* fp=FKL_VM_FP(vfp);
	if(fp->fp==NULL||fklUninitVMfp(fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	fp->fp=NULL;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

typedef enum
{
	PARSE_CONTINUE=0,
	PARSE_DONE,
	PARSE_REDUCING,
}ParsingState;

typedef struct
{
	FklStringBuffer buf;
	FklVMvalue* vfp;
	FklVMvalue* parser;
	FklPtrStack* symbolStack;
	FklUintStack* lineStack;
	FklPtrStack* stateStack;
	FklSid_t reducing_sid;
	FklVMasyncReadCtx* async_read_ctx;
	uint32_t offset;
	uint32_t state;
}ReadCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReadCtx);

static void do_nothing_print_backtrace(void* data,FILE* fp,FklSymbolTable* table)
{
}

static inline void _eof_userdata_princ(const FklVMudata* ud,FILE* fp,FklSymbolTable* table)
{
	fprintf(fp,"#<eof>");
}

static FklVMudMetaTable EofUserDataMetaTable=
{
	.size=0,
	.__princ=_eof_userdata_princ,
	.__prin1=_eof_userdata_princ,
	.__finalizer=NULL,
	.__equal=NULL,
	.__call=NULL,
	.__cmp=NULL,
	.__write=NULL,
	.__atomic=NULL,
	.__append=NULL,
	.__copy=NULL,
	.__length=NULL,
	.__hash=NULL,
};

FklVMvalue* create_eof_value(FklVM* exe)
{
	return fklCreateVMvalueUdata(exe,&EofUserDataMetaTable,NULL);
}

static void read_frame_atomic(void* data,FklVMgc* gc)
{
	ReadCtx* c=(ReadCtx*)data;
	fklVMgcToGray(c->vfp,gc);
	fklVMgcToGray(c->parser,gc);
	FklAnalysisSymbol** base=(FklAnalysisSymbol**)c->symbolStack->base;
	size_t len=c->symbolStack->top;
	for(size_t i=0;i<len;i++)
		fklVMgcToGray(base[i]->ast,gc);
}

static void read_frame_finalizer(void* data)
{
	ReadCtx* c=(ReadCtx*)data;
	fklUnLockVMfp(c->vfp);
	fklUninitStringBuffer(&c->buf);
	FklPtrStack* ss=c->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
		free(fklPopPtrStack(ss));
	fklDestroyPtrStack(ss);
	fklDestroyUintStack(c->lineStack);
	fklDestroyPtrStack(c->stateStack);
	free(c->async_read_ctx);
}

static int read_frame_end(void* d)
{
	return ((ReadCtx*)d)->state==PARSE_DONE;
}

static void read_frame_step(void* d,FklVM* exe)
{
	ReadCtx* rctx=(ReadCtx*)d;
	FklVMfp* vfp=FKL_VM_FP(rctx->vfp);
	FklStringBuffer* s=&rctx->buf;
	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);

	int err=0;
	size_t restLen=fklStringBufferLen(s)-rctx->offset;
	size_t errLine=0;
	FklVMvalue* ast=fklDefaultParseForCharBuf(fklStringBufferBody(s)+rctx->offset
			,restLen
			,&restLen
			,&outerCtx
			,exe->symbolTable
			,&err
			,&errLine
			,rctx->symbolStack
			,rctx->lineStack
			,rctx->stateStack);
	rctx->offset=fklStringBufferLen(s)-restLen;

	if(rctx->symbolStack->top==0&&fklVMfpEof(vfp))
	{
		rctx->state=PARSE_DONE;
		FKL_VM_PUSH_VALUE(exe,create_eof_value(exe));
	}
	else if((err==FKL_PARSE_WAITING_FOR_MORE
				||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			&&fklVMfpEof(vfp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTED_EOF,exe);
	else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
	else if(err==FKL_PARSE_REDUCE_FAILED)
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
	else if(ast)
	{
		if(restLen)
			fklVMfpRewind(vfp,s,fklStringBufferLen(s)-restLen);
		rctx->state=PARSE_DONE;
		FKL_VM_PUSH_VALUE(exe,ast);
	}
	else
		fklVMread(exe,rctx->async_read_ctx);
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

static inline FklAnalysisSymbol* create_nonterm_analyzing_symbol(FklSid_t id,FklVMvalue* ast)
{
	FklAnalysisSymbol* sym=(FklAnalysisSymbol*)malloc(sizeof(FklAnalysisSymbol));
	FKL_ASSERT(sym);
	sym->nt.group=0;
	sym->nt.sid=id;
	sym->ast=ast;
	return sym;
}

static inline void push_state0_of_custom_parser(FklVMvalue* parser,FklPtrStack* stack)
{
	FKL_DECL_UD_DATA(g,FklGrammer,FKL_VM_UD(parser));
	fklPushPtrStack(&g->aTable.states[0],stack);
}

static inline void initReadCtx(void* data
		,FklVM* exe
		,FklVMvalue* vfp
		,FklVMvalue* parser)
{
	ReadCtx* ctx=(ReadCtx*)data;
	ctx->parser=parser;
	ctx->vfp=vfp;
	ctx->symbolStack=fklCreatePtrStack(16,16);
	ctx->stateStack=fklCreatePtrStack(16,16);
	ctx->lineStack=fklCreateUintStack(16,16);
	if(parser==FKL_VM_NIL)
		fklVMvaluePushState0ToStack(ctx->stateStack);
	else
		push_state0_of_custom_parser(parser,ctx->stateStack);
	fklInitStringBuffer(&ctx->buf);
	ctx->state=PARSE_CONTINUE;
	ctx->offset=0;
	ctx->reducing_sid=0;
	ctx->async_read_ctx=fklCreateVMasyncReadCtx(exe,FKL_VM_FP(vfp)->fp,&ctx->buf,1,'\n');
	fklVMread(exe,ctx->async_read_ctx);
}

static inline int do_custom_parser_reduce_action(FklPtrStack* stateStack
		,FklPtrStack* symbolStack
		,FklUintStack* lineStack
		,const FklGrammerProduction* prod
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st
		,size_t* errLine)
{
	size_t len=prod->len;
	stateStack->top-=len;
	FklAnalysisStateGoto* gt=((const FklAnalysisState*)fklTopPtrStack(stateStack))->state.gt;
	const FklAnalysisState* state=NULL;
	FklSid_t left=prod->left.sid;
	for(;gt;gt=gt->next)
		if(gt->nt.sid==left)
			state=gt->state;
	if(!state)
		return 1;
	symbolStack->top-=len;
	void** nodes=NULL;
	if(len)
	{
		nodes=(void**)malloc(sizeof(void*)*len);
		FKL_ASSERT(nodes);
		FklAnalysisSymbol** base=(FklAnalysisSymbol**)&symbolStack->base[symbolStack->top];
		for(size_t i=0;i<len;i++)
		{
			FklAnalysisSymbol* as=base[i];
			nodes[i]=as->ast;
			free(as);
		}
	}
	size_t line=fklGetFirstNthLine(lineStack,len,outerCtx->line);
	lineStack->top-=len;
	prod->func(prod->ctx,outerCtx->ctx,nodes,len,line,st);
	if(len)
	{
		for(size_t i=0;i<len;i++)
			outerCtx->destroy(nodes[i]);
		free(nodes);
	}
	fklPushUintStack(line,lineStack);
	fklPushPtrStack((void*)state,stateStack);
	return 0;
}

static inline void parse_with_custom_parser_for_char_buf(const FklGrammer* g
		,const char* cstr
		,size_t len
		,size_t* restLen
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st
		,int* err
		,size_t* errLine
		,FklPtrStack* symbolStack
		,FklUintStack* lineStack
		,FklPtrStack* stateStack
		,FklVM* exe
		,int* accept
		,ParsingState* parse_state
		,FklSid_t* reducing_sid)
{
	*restLen=len;
	const char* start=cstr;
	size_t matchLen=0;
	int waiting_for_more_err=0;
	for(;;)
	{
		int is_waiting_for_more=0;
		const FklAnalysisState* state=fklTopPtrStack(stateStack);
		const FklAnalysisStateAction* action=state->state.action;
		for(;action;action=action->next)
			if(fklIsStateActionMatch(&action->match,start,cstr,*restLen,&matchLen,outerCtx,&is_waiting_for_more,g))
				break;
		waiting_for_more_err|=is_waiting_for_more;
		if(action)
		{
			switch(action->action)
			{
				case FKL_ANALYSIS_IGNORE:
					outerCtx->line+=fklCountCharInBuf(cstr,matchLen,'\n');
					cstr+=matchLen;
					(*restLen)-=matchLen;
					continue;
					break;
				case FKL_ANALYSIS_SHIFT:
					fklPushPtrStack((void*)action->state,stateStack);
					fklPushPtrStack(fklCreateTerminalAnalysisSymbol(cstr,matchLen,outerCtx),symbolStack);
					fklPushUintStack(outerCtx->line,lineStack);
					outerCtx->line+=fklCountCharInBuf(cstr,matchLen,'\n');
					cstr+=matchLen;
					(*restLen)-=matchLen;
					break;
				case FKL_ANALYSIS_ACCEPT:
					*accept=1;
					return;
					break;
				case FKL_ANALYSIS_REDUCE:
					*parse_state=PARSE_REDUCING;
					*reducing_sid=action->prod->left.sid;
					if(do_custom_parser_reduce_action(stateStack
								,symbolStack
								,lineStack
								,action->prod
								,outerCtx
								,st
								,errLine))
						*err=FKL_PARSE_REDUCE_FAILED;
					return;
					break;
			}
		}
		else
		{
			*err=waiting_for_more_err?FKL_PARSE_WAITING_FOR_MORE:FKL_PARSE_TERMINAL_MATCH_FAILED;
			break;
		}
	}
	return;
}

static void custom_read_frame_step(void* d,FklVM* exe)
{
	ReadCtx* rctx=(ReadCtx*)d;
	FklVMfp* vfp=FKL_VM_FP(rctx->vfp);
	FklStringBuffer* s=&rctx->buf;

	if(rctx->state==PARSE_REDUCING)
	{
		FklVMvalue* ast=fklPopTopValue(exe);
		fklPushPtrStack(create_nonterm_analyzing_symbol(rctx->reducing_sid,ast),rctx->symbolStack);
		rctx->state=PARSE_CONTINUE;
	}

	FKL_DECL_UD_DATA(g,FklGrammer,FKL_VM_UD(rctx->parser));

	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);

	int err=0;
	int accept=0;
	size_t restLen=fklStringBufferLen(s)-rctx->offset;
	size_t errLine=0;

	parse_with_custom_parser_for_char_buf(g
			,fklStringBufferBody(s)+rctx->offset
			,restLen
			,&restLen
			,&outerCtx
			,exe->symbolTable
			,&err
			,&errLine
			,rctx->symbolStack
			,rctx->lineStack
			,rctx->stateStack
			,exe
			,&accept
			,&rctx->state
			,&rctx->reducing_sid);

	if(accept)
	{
		if(restLen)
			fklVMfpRewind(vfp,s,fklStringBufferLen(s)-restLen);
		rctx->state=PARSE_DONE;
		FklAnalysisSymbol* top=fklPopPtrStack(rctx->symbolStack);
		FKL_VM_PUSH_VALUE(exe,top->ast);
		free(top);
	}
	else if(rctx->symbolStack->top==0&&fklVMfpEof(vfp))
	{
		rctx->state=PARSE_DONE;
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else if((err==FKL_PARSE_WAITING_FOR_MORE
				||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			&&fklVMfpEof(vfp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTED_EOF,exe);
	else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
	else if(err==FKL_PARSE_REDUCE_FAILED)
		FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
	else
	{
		rctx->offset=fklStringBufferLen(s)-restLen;
		if(rctx->state==PARSE_CONTINUE)
			fklVMread(exe,rctx->async_read_ctx);
	}
}

static const FklVMframeContextMethodTable CustomReadContextMethodTable=
{
	.atomic=read_frame_atomic,
	.finalizer=read_frame_finalizer,
	.copy=NULL,
	.print_backtrace=do_nothing_print_backtrace,
	.step=custom_read_frame_step,
	.end=read_frame_end,
};

static inline void init_custom_read_frame(FklVM* exe,FklVMvalue* parser,FklVMvalue* vfp)
{
	fklLockVMfp(vfp,exe);
	FklVMframe* f=exe->frames;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&CustomReadContextMethodTable;
	initReadCtx(f->data,exe,vfp,parser);
}

static inline void initFrameToReadFrame(FklVM* exe,FklVMvalue* vfp)
{
	fklLockVMfp(vfp,exe);
	FklVMframe* f=exe->frames;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&ReadContextMethodTable;
	initReadCtx(f->data,exe,vfp,FKL_VM_NIL);
}

#define FKL_VM_FP_R_MASK (1)
#define FKL_VM_FP_W_MASK (2)

static inline int isVMfpReadable(const FklVMvalue* fp)
{
	return FKL_VM_FP(fp)->rw&FKL_VM_FP_R_MASK;
}

static inline int isVMfpWritable(const FklVMvalue* fp)
{
	return FKL_VM_FP(fp)->rw&FKL_VM_FP_W_MASK;
}

#undef FKL_VM_FP_R_MASK
#undef FKL_VM_FP_W_MASK

FKL_VM_USER_DATA_DEFAULT_PRINT(custom_parser_print,parser);
FKL_VM_USER_DATA_DEFAULT_TO_STRING(custom_parser_to_string,parser);

static void custom_parser_finalizer(FklVMudata* p)
{
	FKL_DECL_UD_DATA(grammer,FklGrammer,p);
	fklUninitGrammer(grammer);
}

static void custom_parser_atomic(const FklVMudata* p,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(g,FklGrammer,p);
	for(FklHashTableItem* item=g->productions.first;item;item=item->next)
	{
		FklGrammerProdHashItem* prod_item=(FklGrammerProdHashItem*)item->data;
		for(FklGrammerProduction* prod=prod_item->prods;prod;prod=prod->next)
			fklVMgcToGray(prod->ctx,gc);
	}
}

static void* custom_parser_prod_action(void* ctx
		,void* outerCtx
		,void* asts[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklVMvalue* proc=(FklVMvalue*)ctx;
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* line_value=line>FKL_FIX_INT_MAX?fklCreateVMvalueBigIntWithU64(exe,line):FKL_MAKE_VM_FIX(line);
	FklVMvalue* vect=fklCreateVMvalueVec(exe,num);
	FklVMvec* vec=FKL_VM_VEC(vect);
	for(size_t i=0;i<num;i++)
		vec->base[i]=asts[i];
	fklSetBp(exe);
	FKL_VM_PUSH_VALUE(exe,line_value);
	FKL_VM_PUSH_VALUE(exe,vect);
	fklCallObj(exe,proc);
	return NULL;
}

typedef struct
{
	FklVMvalue* box;
	FklVMvalue* parser;
	FklVMvalue* str;
	FklPtrStack* symbolStack;
	FklUintStack* lineStack;
	FklPtrStack* stateStack;
	size_t offset;
	ParsingState state;
	FklSid_t reducing_sid;
}CustomParseCtx;

static const FklVMudMetaTable CustomParserMetaTable=
{
	.size=sizeof(FklGrammer),
	.__princ=custom_parser_print,
	.__prin1=custom_parser_print,
	.__to_string=custom_parser_to_string,
	.__atomic=custom_parser_atomic,
	.__finalizer=custom_parser_finalizer,
};

static inline FklGrammerProduction* vm_vec_to_prod(FklVMvec* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklSid_t left)
{

	FklPtrStack valid_items;
	fklInitPtrStack(&valid_items,vec->size,8);

	uint8_t* delim=(uint8_t*)malloc(sizeof(uint8_t)*vec->size);
	FKL_ASSERT(delim);
	memset(delim,1,sizeof(uint8_t)*vec->size);

	uint8_t* end_with_terminal=(uint8_t*)malloc(sizeof(uint8_t)*vec->size);
	memset(end_with_terminal,0,sizeof(uint8_t)*vec->size);

	FklGrammerProduction* prod=NULL;

	for(size_t i=0;i<vec->size;i++)
	{
		FklVMvalue* cur=vec->base[i];
		if(FKL_IS_STR(cur))
			fklPushPtrStack((void*)cur,&valid_items);
		else if(FKL_IS_SYM(cur))
			fklPushPtrStack((void*)cur,&valid_items);
		else if(FKL_IS_BOX(cur)&&FKL_IS_STR(FKL_VM_BOX(cur)))
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur==FKL_VM_NIL)
		{
			delim[valid_items.top]=0;
			continue;
		}
		else if(cur==FKL_VM_TRUE)
		{
			end_with_terminal[valid_items.top]=1;
			continue;
		}
		else
			goto end;
	}
	FklGrammerSym* syms=NULL;
	if(valid_items.top)
	{
		size_t top=valid_items.top;
		syms=(FklGrammerSym*)malloc(sizeof(FklGrammerSym)*valid_items.top);
		FKL_ASSERT(syms);
		FklVMvalue** base=(FklVMvalue**)valid_items.base;
		for(size_t i=0;i<top;i++)
		{
			FklVMvalue* cur=base[i];
			FklGrammerSym* ss=&syms[i];
			ss->delim=delim[i];
			ss->end_with_terminal=0;
			if(FKL_IS_STR(cur))
			{
				ss->isterm=1;
				ss->term_type=FKL_TERM_STRING;
				ss->nt.group=0;
				ss->nt.sid=fklAddSymbol(FKL_VM_STR(cur),tt)->id;
				ss->end_with_terminal=end_with_terminal[i];
			}
			else if(FKL_IS_BOX(cur))
			{
				const FklString* str=FKL_VM_STR(FKL_VM_BOX(cur));
				ss->isterm=1;
				ss->term_type=FKL_TERM_REGEX;
				const FklRegexCode* re=fklAddRegexStr(rt,str);
				if(!re)
				{
					free(syms);
					goto end;
				}
				ss->re=re;
			}
			else
			{
				FklSid_t id=FKL_GET_SYM(cur);
				const FklLalrBuiltinMatch* builtin_match=fklGetBuiltinMatch(builtin_term,id);
				if(builtin_match)
				{
					ss->isterm=1;
					ss->term_type=FKL_TERM_BUILTIN;
					ss->b.t=builtin_match;
					ss->b.c=NULL;
				}
				else
				{
					ss->isterm=0;
					ss->term_type=FKL_TERM_STRING;
					ss->nt.group=0;
					ss->nt.sid=id;
				}
			}
		}
	}
	prod=fklCreateProduction(0
			,left
			,valid_items.top
			,syms
			,NULL
			,custom_parser_prod_action
			,NULL
			,fklProdCtxDestroyDoNothing
			,fklProdCtxCopyerDoNothing);

end:
	fklUninitPtrStack(&valid_items);
	free(delim);
	free(end_with_terminal);
	return prod;
}

static int builtin_make_parser(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-parser";
	DECL_AND_CHECK_ARG(start,Pname);
	FKL_CHECK_TYPE(start,FKL_IS_SYM,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueUdata(exe,&CustomParserMetaTable,FKL_VM_NIL);
	FKL_DECL_UD_DATA(grammer,FklGrammer,FKL_VM_UD(retval));
	fklInitEmptyGrammer(grammer,exe->symbolTable);
	grammer->start=(FklGrammerNonterm){.group=0,.sid=FKL_GET_SYM(start)};

	if(fklAddExtraProdToGrammer(grammer))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_GRAMMER_CREATE_FAILED,exe);

#define EXCEPT_NEXT_ARG_SYMBOL (0)
#define EXCEPT_NEXT_ARG_VECTOR (1)
#define EXCEPT_NEXT_ARG_CALLABLE (2)

	int next=EXCEPT_NEXT_ARG_SYMBOL;
	FklVMvalue* next_arg=FKL_VM_POP_ARG(exe);
	FklGrammerProduction* prod=NULL;
	int is_adding_ignore=0;
	FklSid_t sid=0;
	FklSymbolTable* tt=&grammer->terminals;
	FklRegexTable* rt=&grammer->regexes;
	FklHashTable* builtins=&grammer->builtins;
	for(;next_arg;next_arg=FKL_VM_POP_ARG(exe))
	{
		switch(next)
		{
			case EXCEPT_NEXT_ARG_SYMBOL:
				next=EXCEPT_NEXT_ARG_VECTOR;
				if(next_arg==FKL_VM_NIL)
				{
					is_adding_ignore=1;
					sid=0;
				}
				else if(FKL_IS_SYM(next_arg))
				{
					is_adding_ignore=0;
					sid=FKL_GET_SYM(next_arg);
				}
				else
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				break;
			case EXCEPT_NEXT_ARG_VECTOR:
				if(FKL_IS_VECTOR(next_arg))
				{
					prod=vm_vec_to_prod(FKL_VM_VEC(next_arg),builtins,tt,rt,sid);
					if(!prod)
						FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_REGEX_COMPILE_FAILED,exe);
					next=EXCEPT_NEXT_ARG_CALLABLE;
					if(is_adding_ignore)
					{
						FklGrammerIgnore* ig=fklGrammerSymbolsToIgnore(prod->syms,prod->len,tt);
						fklDestroyGrammerProduction(prod);
						if(!ig)
							FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_GRAMMER_CREATE_FAILED,exe);
						next_arg=EXCEPT_NEXT_ARG_SYMBOL;
						if(fklAddIgnoreToIgnoreList(&grammer->ignores,ig))
						{
							fklDestroyIgnore(ig);
							FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_GRAMMER_CREATE_FAILED,exe);
						}
						next=EXCEPT_NEXT_ARG_SYMBOL;
					}
				}
				else
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				break;
			case EXCEPT_NEXT_ARG_CALLABLE:
				next=EXCEPT_NEXT_ARG_SYMBOL;
				if(fklIsCallable(next_arg))
				{
					prod->ctx=next_arg;
					if(fklAddProdAndExtraToGrammer(grammer,prod))
					{
						fklDestroyGrammerProduction(prod);
						FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_GRAMMER_CREATE_FAILED,exe);
					}
				}
				else
				{
					fklDestroyGrammerProduction(prod);
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				}
				break;
		}
	}
	if(next!=EXCEPT_NEXT_ARG_SYMBOL)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
	fklResBp(exe);

#undef EXCEPT_NEXT_ARG_SYMBOL
#undef EXCEPT_NEXT_ARG_VECTOR
#undef EXCEPT_NEXT_ARG_CALLABLE

	if(fklCheckAndInitGrammerSymbols(grammer))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_GRAMMER_CREATE_FAILED,exe);

	FklHashTable* itemSet=fklGenerateLr0Items(grammer);
	fklLr0ToLalrItems(itemSet,grammer);

	if(fklGenerateLalrAnalyzeTable(grammer,itemSet))
	{
		fklDestroyHashTable(itemSet);
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_ANALYSIS_TABLE_GENERATE_FAILED,exe);
	}
	fklDestroyHashTable(itemSet);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static inline int is_custom_parser(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&CustomParserMetaTable;
}

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(CustomParseCtx);

static void custom_parse_frame_atomic(void* data,FklVMgc* gc)
{
	CustomParseCtx* c=(CustomParseCtx*)data;
	fklVMgcToGray(c->box,gc);
	fklVMgcToGray(c->parser,gc);
	fklVMgcToGray(c->str,gc);
	FklAnalysisSymbol** base=(FklAnalysisSymbol**)c->symbolStack->base;
	size_t len=c->symbolStack->top;
	for(size_t i=0;i<len;i++)
		fklVMgcToGray(base[i]->ast,gc);
}

static void custom_parse_frame_finalizer(void* data)
{
	CustomParseCtx* c=(CustomParseCtx*)data;
	FklPtrStack* ss=c->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
		free(fklPopPtrStack(ss));
	fklDestroyPtrStack(ss);
	fklDestroyUintStack(c->lineStack);
	fklDestroyPtrStack(c->stateStack);
}

static int custom_parse_frame_end(void* d)
{
	return ((CustomParseCtx*)d)->state==PARSE_DONE;
}

static void custom_parse_frame_step(void* d,FklVM* exe)
{
	CustomParseCtx* ctx=(CustomParseCtx*)d;
	if(ctx->state==PARSE_REDUCING)
	{
		FklVMvalue* ast=fklPopTopValue(exe);
		fklPushPtrStack(create_nonterm_analyzing_symbol(ctx->reducing_sid,ast),ctx->symbolStack);
		ctx->state=PARSE_CONTINUE;
	}
	FKL_DECL_UD_DATA(g,FklGrammer,FKL_VM_UD(ctx->parser));
	FklString* str=FKL_VM_STR(ctx->str);
	int err=0;
	uint64_t errLine=0;
	int accept=0;
	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);
	size_t restLen=str->size-ctx->offset;
	parse_with_custom_parser_for_char_buf(g
			,str->str+ctx->offset
			,restLen
			,&restLen
			,&outerCtx
			,exe->symbolTable
			,&err
			,&errLine
			,ctx->symbolStack
			,ctx->lineStack
			,ctx->stateStack
			,exe
			,&accept
			,&ctx->state
			,&ctx->reducing_sid);
	if(err)
	{
		if(err==FKL_PARSE_WAITING_FOR_MORE||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			FKL_RAISE_BUILTIN_ERROR_CSTR("parsing",FKL_ERR_UNEXPECTED_EOF,exe);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR("parsing",FKL_ERR_INVALIDEXPR,exe);
	}
	if(accept)
	{
		ctx->state=PARSE_DONE;
		FklAnalysisSymbol* top=fklPopPtrStack(ctx->symbolStack);
		FKL_VM_PUSH_VALUE(exe,top->ast);
		free(top);
		if(ctx->box)
		{
			uint64_t offset=ctx->offset=str->size-restLen;
			if(offset>FKL_FIX_INT_MAX)
				FKL_VM_BOX(ctx->box)=fklCreateVMvalueBigIntWithU64(exe,offset);
			else
				FKL_VM_BOX(ctx->box)=FKL_MAKE_VM_FIX(offset);
		}
	}
	else
		ctx->offset=str->size-restLen;
}

static inline void init_custom_parse_ctx(void* data
		,FklVMvalue* parser
		,FklVMvalue* str
		,FklVMvalue* box)
{
	CustomParseCtx* ctx=(CustomParseCtx*)data;
	ctx->box=box;
	ctx->parser=parser;
	ctx->str=str;
	ctx->symbolStack=fklCreatePtrStack(16,16);
	ctx->stateStack=fklCreatePtrStack(16,16);
	ctx->lineStack=fklCreateUintStack(16,16);
	push_state0_of_custom_parser(parser,ctx->stateStack);
	ctx->state=PARSE_CONTINUE;
	ctx->offset=0;
}


static const FklVMframeContextMethodTable CustomParseContextMethodTable=
{
	.atomic=custom_parse_frame_atomic,
	.finalizer=custom_parse_frame_finalizer,
	.print_backtrace=do_nothing_print_backtrace,
	.step=custom_parse_frame_step,
	.end=custom_parse_frame_end,
};

static inline void init_custom_parse_frame(FklVM* exe
		,FklVMvalue* parser
		,FklVMvalue* str
		,FklVMvalue* box)
{
	FklVMframe* f=exe->frames;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&CustomParseContextMethodTable;
	init_custom_parse_ctx(f->data,parser,str,box);
}

#define CHECK_FP_READABLE(V,I,E) if(!isVMfpReadable(V))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(I,FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_WRITABLE(V,I,E) if(!isVMfpWritable(V))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(I,FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_OPEN(V,I,E) if(!FKL_VM_FP(V)->fp)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(I,FKL_ERR_INVALIDACCESS,E)

#define GET_OR_USE_STDOUT(VN) if(!VN)\
	VN=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->sysOut

#define GET_OR_USE_STDIN(VN) if(!VN)\
	VN=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->sysIn

static int builtin_read(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.read";
	FklVMvalue* stream=FKL_VM_POP_ARG(exe);
	FklVMvalue* parser=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if((stream&&!FKL_IS_FP(stream))||(parser&&!is_custom_parser(parser)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(!stream||FKL_IS_FP(stream))
	{
		FKL_DECL_UD_DATA(pbd,PublicBuiltInData,FKL_VM_UD(ctx->pd));
		FklVMvalue* fpv=stream?stream:pbd->sysIn;
		CHECK_FP_READABLE(fpv,Pname,exe);
		CHECK_FP_OPEN(fpv,Pname,exe);
		if(parser)
		{
			FklVMframe* sf=exe->frames;
			FklVMframe* nf=fklCreateOtherObjVMframe(sf->t,sf->prev);
			exe->frames=nf;
			init_custom_read_frame(exe,parser,stream);
		}
		else
			initFrameToReadFrame(exe,fpv);
	}
	return 1;
}

static int builtin_stringify(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.stringify";
	DECL_AND_CHECK_ARG(v,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,fklStringify(v,exe->symbolTable));
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_parse(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.parse";
	DECL_AND_CHECK_ARG(str,Pname);
	if(!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	FklVMvalue* custom_parser=NULL;
	FklVMvalue* box=NULL;
	FklVMvalue* next_arg=FKL_VM_POP_ARG(exe);
	if(next_arg)
	{
		if(FKL_IS_BOX(next_arg)&&!box)
			box=next_arg;
		else if(is_custom_parser(next_arg)&&!custom_parser)
			custom_parser=next_arg;
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	next_arg=FKL_VM_POP_ARG(exe);
	if(next_arg)
	{
		if(FKL_IS_BOX(next_arg)&&!box)
			box=next_arg;
		else if(is_custom_parser(next_arg)&&!custom_parser)
			custom_parser=next_arg;
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}

	FKL_CHECK_REST_ARG(exe,Pname);

	if(custom_parser)
	{
		FKL_CHECK_TYPE(custom_parser,is_custom_parser,Pname,exe);
		FklVMframe* sf=exe->frames;
		FklVMframe* nf=fklCreateOtherObjVMframe(sf->t,sf->prev);
		exe->frames=nf;
		init_custom_parse_frame(exe,custom_parser,str,box);
		return 1;
	}
	else
	{
		FklString* ss=FKL_VM_STR(str);
		int err=0;
		size_t errorLine=0;
		FklPtrStack symbolStack;
		FklPtrStack stateStack;
		FklUintStack lineStack;
		fklInitPtrStack(&symbolStack,16,16);
		fklInitPtrStack(&stateStack,16,16);
		fklInitUintStack(&lineStack,16,16);
		fklVMvaluePushState0ToStack(&stateStack);

		size_t restLen=ss->size;
		FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);
		FklVMvalue* node=fklDefaultParseForCharBuf(ss->str
				,restLen
				,&restLen
				,&outerCtx
				,exe->symbolTable
				,&err
				,&errorLine
				,&symbolStack
				,&lineStack
				,&stateStack);

		if(node==NULL)
		{
			while(!fklIsPtrStackEmpty(&symbolStack))
				free(fklPopPtrStack(&symbolStack));
			fklUninitPtrStack(&symbolStack);
			fklUninitPtrStack(&stateStack);
			fklUninitUintStack(&lineStack);
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDEXPR,exe);
		}

		while(!fklIsPtrStackEmpty(&symbolStack))
			free(fklPopPtrStack(&symbolStack));
		fklUninitPtrStack(&symbolStack);
		fklUninitPtrStack(&stateStack);
		fklUninitUintStack(&lineStack);
		FKL_VM_PUSH_VALUE(exe,node);
		if(box)
		{
			uint64_t offset=ss->size-restLen;
			if(offset>FKL_FIX_INT_MAX)
				FKL_VM_BOX(box)=fklCreateVMvalueBigIntWithU64(exe,offset);
			else
				FKL_VM_BOX(box)=FKL_MAKE_VM_FIX(offset);
		}
		return 0;
	}
}

typedef struct
{
	FklVMvalue* vfp;
	FklStringBuffer buf;
	FklVMasyncReadCtx* async_read_ctx;
	FklVMvalue* (*retval_creator)(FklVM*,FklStringBuffer*);
	int done;
}AsyncFgetCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(AsyncFgetCtx);

static void async_fget_frame_atomic(void* data,FklVMgc* gc)
{
	AsyncFgetCtx* c=(AsyncFgetCtx*)data;
	fklVMgcToGray(c->vfp,gc);
}

static void async_fget_frame_finalizer(void* data)
{
	AsyncFgetCtx* c=(AsyncFgetCtx*)data;
	fklUninitStringBuffer(&c->buf);
	fklUnLockVMfp(c->vfp);
	free(c->async_read_ctx);
}

static void async_fget_frame_step(void* data,FklVM* exe)
{
	AsyncFgetCtx* c=(AsyncFgetCtx*)data;
	FKL_VM_PUSH_VALUE(exe,c->retval_creator(exe,&c->buf));
	fklUnLockVMfp(c->vfp);
	c->done=1;
}

static int async_fget_frame_end(void* data)
{
	AsyncFgetCtx* c=(AsyncFgetCtx*)data;
	return c->done;
}

static const FklVMframeContextMethodTable AsyncFgetContextMethodTable=
{
	.atomic=async_fget_frame_atomic,
	.finalizer=async_fget_frame_finalizer,
	.copy=NULL,
	.print_backtrace=do_nothing_print_backtrace,
	.step=async_fget_frame_step,
	.end=async_fget_frame_end,
};

static void initAsyncFgetCtx(void* data
		,FklVM* exe
		,FklVMvalue* vfp
		,uint64_t len
		,int d
		,FklVMvalue* (*retval_creator)(FklVM* exe,FklStringBuffer* buf))
{
	AsyncFgetCtx* ctx=(AsyncFgetCtx*)data;
	ctx->vfp=vfp;
	ctx->done=0;
	ctx->retval_creator=retval_creator;
	FklStringBuffer* buf=&ctx->buf;
	fklInitStringBufferWithCapacity(buf,len);
	ctx->async_read_ctx=fklCreateVMasyncReadCtx(exe,FKL_VM_FP(vfp)->fp,buf,len,d);
	fklVMread(exe,ctx->async_read_ctx);
}

static FklVMvalue* async_char_creator(FklVM* exe,FklStringBuffer* buf)
{
	if(buf->index)
		return FKL_MAKE_VM_CHR(*(buf->buf));
	return FKL_VM_NIL;
}

static FklVMvalue* async_integer_creator(FklVM* exe,FklStringBuffer* buf)
{
	if(buf->index)
		return FKL_MAKE_VM_FIX(*(buf->buf));
	return FKL_VM_NIL;
}

static FklVMvalue* async_string_creator(FklVM* exe,FklStringBuffer* buf)
{
	return fklCreateVMvalueStr(exe,fklStringBufferToString(buf));
}

static FklVMvalue* async_bvec_creator(FklVM* exe,FklStringBuffer* buf)
{
	return fklCreateVMvalueBvec(exe,fklStringBufferToBytevector(buf));
}

static void initFrameToAsyncFgetFrame(FklVM* exe
		,FklVMvalue* vfp
		,uint64_t len
		,int d
		,FklVMvalue* (*retval_creator)(FklVM* exe,FklStringBuffer* buf))
{
	FklVMframe* f=exe->frames;
	fklLockVMfp(vfp,exe);
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&AsyncFgetContextMethodTable;
	initAsyncFgetCtx(f->data,exe,vfp,len,d,retval_creator);
}

static int builtin_fgetc(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fgetc";
	FklVMvalue* stream=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDIN(stream);
	FKL_CHECK_TYPE(stream,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(stream,Pname,exe);
	CHECK_FP_READABLE(stream,Pname,exe);
	initFrameToAsyncFgetFrame(exe,stream,1,EOF,async_char_creator);
	return 1;
}

static int builtin_fgeti(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fgeti";
	FklVMvalue* stream=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDIN(stream);
	FKL_CHECK_TYPE(stream,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(stream,Pname,exe);
	CHECK_FP_READABLE(stream,Pname,exe);
	initFrameToAsyncFgetFrame(exe,stream,1,EOF,async_integer_creator);
	return 1;
}

static int builtin_fgetd(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fgetd";
	FklVMvalue* del=FKL_VM_POP_ARG(exe);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	int d=EOF;
	if(del)
	{
		FKL_CHECK_TYPE(del,FKL_IS_CHR,Pname,exe);
		d=FKL_GET_CHR(del);
	}
	else
		d='\n';
	GET_OR_USE_STDIN(file);
	FKL_CHECK_TYPE(file,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_READABLE(file,Pname,exe);
	initFrameToAsyncFgetFrame(exe,file,1,d,async_string_creator);
	return 1;
}

static int builtin_fgets(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fgets";
	DECL_AND_CHECK_ARG(psize,Pname);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!fklIsVMint(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_READABLE(file,Pname,exe);
	initFrameToAsyncFgetFrame(exe,file,fklGetUint(psize),EOF,async_string_creator);
	return 1;
}

static int builtin_fgetb(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fgetb";
	DECL_AND_CHECK_ARG(psize,Pname);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!fklIsVMint(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_READABLE(file,Pname,exe);
	initFrameToAsyncFgetFrame(exe,file,fklGetUint(psize),EOF,async_bvec_creator);
	return 1;
}

static int builtin_prin1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.prin1";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDOUT(file);
	if(!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);

	FILE* fp=FKL_VM_FP(file)->fp;
	fklPrin1VMvalue(obj,fp,exe->symbolTable);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_princ(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.princ";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDOUT(file);
	if(!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);

	FILE* fp=FKL_VM_FP(file)->fp;
	fklPrincVMvalue(obj,fp,exe->symbolTable);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_println(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrincVMvalue(obj,stdout,exe->symbolTable);
	fputc('\n',stdout);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_print(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrincVMvalue(obj,stdout,exe->symbolTable);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_fprint(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fprint";
	DECL_AND_CHECK_ARG(f,Pname);
	FKL_CHECK_TYPE(f,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(f,Pname,exe);
	CHECK_FP_WRITABLE(f,Pname,exe);
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrincVMvalue(obj,fp,exe->symbolTable);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_fprin1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fprin1";
	DECL_AND_CHECK_ARG(f,Pname);
	FKL_CHECK_TYPE(f,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(f,Pname,exe);
	CHECK_FP_WRITABLE(f,Pname,exe);

	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrin1VMvalue(obj,fp,exe->symbolTable);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_newline(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.newline";
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	GET_OR_USE_STDOUT(file);
	FKL_CHECK_TYPE(file,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);
	FILE* fp=FKL_VM_FP(file)->fp;
	fputc('\n',fp);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_dlopen(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.dlopen";
	DECL_AND_CHECK_ARG(dllName,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(dllName,FKL_IS_STR,Pname,exe);
	FklString* dllNameStr=FKL_VM_STR(dllName);
	char* errorStr=NULL;
	FklVMvalue* ndll=fklCreateVMvalueDll(exe,dllNameStr->str,&errorStr);
	if(!ndll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname
				,errorStr?errorStr:dllNameStr->str
				,errorStr!=NULL
				,FKL_ERR_LOADDLLFAILD
				,exe);
	fklInitVMdll(ndll,exe);
	FKL_VM_PUSH_VALUE(exe,ndll);
	return 0;
}

static int builtin_dlsym(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.dlsym";
	DECL_AND_CHECK_ARG2(ndll,symbol,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(ndll))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* ss=FKL_VM_STR(symbol);
	FklVMdll* dll=FKL_VM_DLL(ndll);
	FklVMcFunc funcAddress=NULL;
	if(uv_dlsym(&dll->dll,ss->str,(void**)&funcAddress))
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,ss->str,0,FKL_ERR_INVALIDSYMBOL,exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueCproc(exe,funcAddress,ndll,dll->pd,0));
	return 0;
}

static int builtin_argv(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.argv";
	FklVMvalue* retval=NULL;
	FKL_CHECK_REST_ARG(exe,Pname);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&FKL_VM_CDR(*tmp))
		*tmp=fklCreateVMvaluePairWithCar(exe,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(argv[i])));
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static inline FklVMvalue* isSlot(const FklVMvalue* head,const FklVMvalue* v)
{
	if(FKL_IS_PAIR(v)
			&&FKL_VM_CAR(v)==head
			&&FKL_IS_PAIR(FKL_VM_CDR(v))
			&&FKL_VM_CDR(FKL_VM_CDR(v))==FKL_VM_NIL
			&&FKL_IS_SYM(FKL_VM_CAR(FKL_VM_CDR(v))))
		return FKL_VM_CAR(FKL_VM_CDR(v));
	return NULL;
}

int matchPattern(const FklVMvalue* pattern,FklVMvalue* exp,FklHashTable* ht,FklVMgc* gc)
{
	FklVMvalue* slotS=FKL_VM_CAR(pattern);
	FklPtrQueue q0={NULL,NULL,};
	FklPtrQueue q1={NULL,NULL,};
	fklInitPtrQueue(&q0);
	fklInitPtrQueue(&q1);
	fklPushPtrQueue(FKL_VM_CAR(FKL_VM_CDR(pattern)),&q0);
	fklPushPtrQueue(exp,&q1);
	int r=0;
	while(!fklIsPtrQueueEmpty(&q0)&&!fklIsPtrQueueEmpty(&q1))
	{
		FklVMvalue* v0=fklPopPtrQueue(&q0);
		FklVMvalue* v1=fklPopPtrQueue(&q1);
		FklVMvalue* slotV=isSlot(slotS,v0);
		if(slotV)
			fklVMhashTableSet(slotV,v1,ht);
		else if(FKL_IS_BOX(v0)&&FKL_IS_BOX(v1))
		{
			fklPushPtrQueue(FKL_VM_BOX(v0),&q0);
			fklPushPtrQueue(FKL_VM_BOX(v1),&q1);
		}
		else if(FKL_IS_PAIR(v0)&&FKL_IS_PAIR(v1))
		{
			fklPushPtrQueue(FKL_VM_CAR(v0),&q0);
			fklPushPtrQueue(FKL_VM_CDR(v0),&q0);
			fklPushPtrQueue(FKL_VM_CAR(v1),&q1);
			fklPushPtrQueue(FKL_VM_CDR(v1),&q1);
		}
		else if(FKL_IS_VECTOR(v0)
				&&FKL_IS_VECTOR(v1))
		{
			FklVMvec* vec0=FKL_VM_VEC(v0);
			FklVMvec* vec1=FKL_VM_VEC(v1);
			r=vec0->size!=vec1->size;
			if(r)
				break;
			FklVMvalue** b0=vec0->base;
			FklVMvalue** b1=vec1->base;
			size_t size=vec0->size;
			for(size_t i=0;i<size;i++)
			{
				fklPushPtrQueue(b0[i],&q0);
				fklPushPtrQueue(b1[i],&q1);
			}
		}
		else if(FKL_IS_HASHTABLE(v0)
				&&FKL_IS_HASHTABLE(v1))
		{
			FklHashTable* h0=FKL_VM_HASH(v0);
			FklHashTable* h1=FKL_VM_HASH(v1);
			r=h0->t!=h1->t
				||h0->num!=h1->num;
			if(r)
				break;
			FklHashTableItem* hl0=h0->first;
			FklHashTableItem* hl1=h1->first;
			while(h0)
			{
				FklVMhashTableItem* i0=(FklVMhashTableItem*)hl0->data;
				FklVMhashTableItem* i1=(FklVMhashTableItem*)hl1->data;
				fklPushPtrQueue(i0->key,&q0);
				fklPushPtrQueue(i0->v,&q0);
				fklPushPtrQueue(i1->key,&q1);
				fklPushPtrQueue(i1->v,&q1);
				hl0=hl0->next;
				hl1=hl1->next;
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
	FklVMvalue* head=FKL_VM_CAR(p);
	if(!FKL_IS_SYM(head))
		return 0;
	const FklVMvalue* body=FKL_VM_CDR(p);
	if(!FKL_IS_PAIR(body))
		return 0;
	if(FKL_VM_CDR(body)!=FKL_VM_NIL)
		return 0;
	body=FKL_VM_CAR(body);
	FklHashTable* symbolTable=fklCreateSidSet();
	FklPtrStack exe=FKL_STACK_INIT;
	fklInitPtrStack(&exe,32,16);
	fklPushPtrStack((void*)body,&exe);
	while(!fklIsPtrStackEmpty(&exe))
	{
		const FklVMvalue* c=fklPopPtrStack(&exe);
		FklVMvalue* slotV=isSlot(head,c);
		if(slotV)
		{
			FklSid_t sid=FKL_GET_SYM(slotV);
			if(fklGetHashItem(&sid,symbolTable))
			{
				fklDestroyHashTable(symbolTable);
				fklUninitPtrStack(&exe);
				return 0;
			}
			fklPutHashItem(&sid,symbolTable);
		}
		if(FKL_IS_PAIR(c))
		{
			fklPushPtrStack(FKL_VM_CAR(c),&exe);
			fklPushPtrStack(FKL_VM_CDR(c),&exe);
		}
		else if(FKL_IS_BOX(c))
			fklPushPtrStack(FKL_VM_BOX(c),&exe);
		else if(FKL_IS_VECTOR(c))
		{
			FklVMvec* vec=FKL_VM_VEC(c);
			FklVMvalue** base=vec->base;
			size_t size=vec->size;
			for(size_t i=0;i<size;i++)
				fklPushPtrStack(base[i],&exe);
		}
		else if(FKL_IS_HASHTABLE(c))
		{
			for(FklHashTableItem* h=FKL_VM_HASH(c)->first;h;h=h->next)
			{
				FklVMhashTableItem* i=(FklVMhashTableItem*)h->data;
				fklPushPtrStack(i->key,&exe);
				fklPushPtrStack(i->v,&exe);
			}
		}
	}
	fklDestroyHashTable(symbolTable);
	fklUninitPtrStack(&exe);
	return 1;
}

static int builtin_pmatch(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.pmatch";
	DECL_AND_CHECK_ARG2(pattern,exp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!isValidSyntaxPattern(pattern))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDPATTERN,exe);
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* hash=FKL_VM_HASH(r);
	if(matchPattern(pattern,exp,hash,exe->gc))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_go(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.go";
	DECL_AND_CHECK_ARG(threadProc,Pname);
	if(!fklIsCallable(threadProc))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVM* threadVM=fklCreateThreadVM(threadProc
			,exe
			,exe->next
			,exe->libNum
			,exe->libs
			,exe->symbolTable
			,exe->builtinErrorTypeId);
	fklSetBp(threadVM);
	size_t arg_num=exe->tp-exe->bp;
	if(arg_num)
	{
		FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);
		for(size_t i=0;i<arg_num;i++)
			FKL_VM_PUSH_VALUE(threadVM,base[i]);
		exe->tp-=arg_num;
	}
	fklResBp(exe);
	FklVMvalue* chan=threadVM->chan;
	FKL_VM_PUSH_VALUE(exe,chan); 
	fklVMworkStart(threadVM,exe->vmq);
	return 0;
}

static int builtin_chanl(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chanl";
	DECL_AND_CHECK_ARG(maxSize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(maxSize,fklIsVMint,Pname,exe);
	if(fklVMnumberLt0(maxSize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueChanl(exe,fklGetUint(maxSize)));
	return 0;
}

static int builtin_chanl_msg_num(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chanl#msg";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=FKL_VM_CHANL(obj)->messageNum;
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_chanl_send_num(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chanl#send";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklLengthPtrQueue(&FKL_VM_CHANL(obj)->sendq);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_chanl_recv_num(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chanl#recv";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklLengthPtrQueue(&FKL_VM_CHANL(obj)->recvq);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_chanl_full_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chanl#recv";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* retval=NULL;
	if(FKL_IS_CHAN(obj))
	{
		FklVMchanl* ch=FKL_VM_CHANL(obj);
		retval=ch->max>0&&ch->messageNum>=ch->max?FKL_MAKE_VM_FIX(1):FKL_VM_NIL;
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_chanl_msg_to_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chanl-msg->list";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* r=NULL;
	FklVMvalue** cur=&r;
	if(FKL_IS_CHAN(obj))
	{
		for(FklQueueNode* h=FKL_VM_CHANL(obj)->messages.head
				;h
				;h=h->next)
		{
			FklVMvalue* msg=h->data;
			*cur=fklCreateVMvaluePairWithCar(exe,msg);
			cur=&FKL_VM_CDR(*cur);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}



static int builtin_send(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.send";
	DECL_AND_CHECK_ARG2(ch,message,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,Pname,exe);
	fklChanlSend(message,FKL_VM_CHANL(ch),exe);
	FKL_VM_PUSH_VALUE(exe,message);
	return 0;
}

static int builtin_recv(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.recv";
	DECL_AND_CHECK_ARG(ch,Pname);
	FklVMvalue* okBox=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,Pname,exe);
	FklVMchanl* chanl=FKL_VM_CHANL(ch);
	if(okBox)
	{
		FKL_CHECK_TYPE(okBox,FKL_IS_BOX,Pname,exe);
		FklVMvalue* r=FKL_VM_NIL;
		FKL_VM_BOX(okBox)=fklChanlRecvOk(chanl,&r)?FKL_VM_TRUE:FKL_VM_NIL;
		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
		fklChanlRecv(fklPushVMvalue(exe,FKL_VM_NIL),chanl,exe);
	return 0;
}

static int builtin_recv7(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.recv&";
	DECL_AND_CHECK_ARG(ch,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,Pname,exe);
	FklVMchanl* chanl=FKL_VM_CHANL(ch);
	FklVMvalue* r=FKL_VM_NIL;
	if(fklChanlRecvOk(chanl,&r))
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,r));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_error(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.error";
	DECL_AND_CHECK_ARG3(type,where,message,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(where)&&!FKL_IS_STR(where)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklCreateVMvalueError(exe
				,FKL_GET_SYM(type)
				,(FKL_IS_SYM(where))
				?fklGetSymbolWithId(FKL_GET_SYM(where),exe->symbolTable)->symbol
				:FKL_VM_STR(where)
				,fklCopyString(FKL_VM_STR(message))));
	return 0;
}

static int builtin_error_type(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.error-type";
	DECL_AND_CHECK_ARG(err,Pname);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMerror* error=FKL_VM_ERR(err);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(error->type));
	return 0;
}

static int builtin_error_where(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.error-where";
	DECL_AND_CHECK_ARG(err,Pname);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMerror* error=FKL_VM_ERR(err);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCopyString(error->where)));
	return 0;
}

static int builtin_error_msg(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.error-msg";
	DECL_AND_CHECK_ARG(err,Pname);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMerror* error=FKL_VM_ERR(err);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCopyString(error->message)));
	return 0;
}

static int builtin_raise(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.raise";
	DECL_AND_CHECK_ARG(err,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,Pname,exe);
	fklPopVMframe(exe);
	fklRaiseVMerror(err,exe);
	return 0;
}

static int builtin_throw(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.throw";
	DECL_AND_CHECK_ARG3(type,where,message,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(where)&&!FKL_IS_STR(where)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* err=fklCreateVMvalueError(exe
			,FKL_GET_SYM(type)
			,(FKL_IS_SYM(where))
			?fklGetSymbolWithId(FKL_GET_SYM(where),exe->symbolTable)->symbol
			:FKL_VM_STR(where)
			,fklCopyString(FKL_VM_STR(message)));
	fklPopVMframe(exe);
	fklRaiseVMerror(err,exe);
	return 0;
}

typedef struct
{
	FklVMvalue* proc;
	FklVMvalue** errorSymbolLists;
	FklVMvalue** errorHandlers;
	size_t num;
	size_t tp;
}EhFrameContext;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(EhFrameContext);

static void error_handler_frame_print_backtrace(void* data,FILE* fp,FklSymbolTable* table)
{
	EhFrameContext* c=(EhFrameContext*)data;
	FklVMcproc* cproc=FKL_VM_CPROC(c->proc);
	if(cproc->sid)
	{
		fprintf(fp,"at cproc:");
		fklPrintString(fklGetSymbolWithId(cproc->sid,table)->symbol
				,fp);
		fputc('\n',fp);
	}
	else
		fputs("at <cproc>\n",fp);
}

static void error_handler_frame_atomic(void* data,FklVMgc* gc)
{
	EhFrameContext* c=(EhFrameContext*)data;
	fklVMgcToGray(c->proc,gc);
	size_t num=c->num;
	for(size_t i=0;i<num;i++)
	{
		fklVMgcToGray(c->errorSymbolLists[i],gc);
		fklVMgcToGray(c->errorHandlers[i],gc);
	}
}

static void error_handler_frame_finalizer(void* data)
{
	EhFrameContext* c=(EhFrameContext*)data;
	free(c->errorSymbolLists);
	free(c->errorHandlers);
}

static void error_handler_frame_copy(void* d,const void* s,FklVM* exe)
{
	const EhFrameContext* const sc=(const EhFrameContext*)s;
	EhFrameContext* dc=(EhFrameContext*)d;
	dc->proc=sc->proc;
	size_t num=sc->num;
	dc->num=num;
	dc->errorSymbolLists=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*num);
	FKL_ASSERT(dc->errorSymbolLists||!num);
	dc->errorHandlers=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*num);
	FKL_ASSERT(dc->errorHandlers||!num);
	for(size_t i=0;i<num;i++)
	{
		dc->errorSymbolLists[i]=sc->errorSymbolLists[i];
		dc->errorHandlers[i]=sc->errorHandlers[i];
	}
}

static int error_handler_frame_end(void* data)
{
	return 1;
}

static void error_handler_frame_step(void* data,FklVM* exe)
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
	for(;symbolList!=FKL_VM_NIL;symbolList=FKL_VM_CDR(symbolList))
	{
		FklVMvalue* cur=FKL_VM_CAR(symbolList);
		FklSid_t sid=FKL_GET_SYM(cur);
		if(sid==type)
			return 1;
	}
	return 0;
}

static int errorCallBackWithErrorHandler(FklVMframe* f,FklVMvalue* errValue,FklVM* exe)
{
	EhFrameContext* c=(EhFrameContext*)f->data;
	size_t num=c->num;
	FklVMvalue** errSymbolLists=c->errorSymbolLists;
	FklVMerror* err=FKL_VM_ERR(errValue);
	for(size_t i=0;i<num;i++)
	{
		if(isShouldBeHandle(errSymbolLists[i],err->type))
		{
			exe->tp=c->tp;
			fklSetBp(exe);
			FKL_VM_PUSH_VALUE(exe,errValue);
			FklVMframe* topFrame=exe->frames;
			exe->frames=f;
			while(topFrame!=f)
			{
				FklVMframe* cur=topFrame;
				topFrame=topFrame->prev;
				fklDestroyVMframe(cur,exe);
			}
			fklTailCallObj(exe,c->errorHandlers[i]);
			return 1;
		}
	}
	return 0;
}

static int builtin_call_eh(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.call/eh";
#define GET_LIST (0)
#define GET_PROC (1)

	DECL_AND_CHECK_ARG(proc,Pname);
	FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
	FklPtrStack errSymbolLists=FKL_STACK_INIT;
	FklPtrStack errHandlers=FKL_STACK_INIT;
	fklInitPtrStack(&errSymbolLists,32,16);
	fklInitPtrStack(&errHandlers,32,16);
	int state=GET_LIST;
	for(FklVMvalue* v=FKL_VM_POP_ARG(exe)
			;v
			;v=FKL_VM_POP_ARG(exe))
	{
		switch(state)
		{
			case GET_LIST:
				if(!fklIsSymbolList(v))
				{
					fklUninitPtrStack(&errSymbolLists);
					fklUninitPtrStack(&errHandlers);
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname
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
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname
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
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
	}
	fklResBp(exe);
	if(errSymbolLists.top)
	{
		FklVMframe* sf=exe->frames;
		FklVMframe* nf=fklCreateOtherObjVMframe(sf->t,sf->prev);
		nf->errorCallBack=errorCallBackWithErrorHandler;
		fklDoCopyObjFrameContext(sf,nf,exe);
		exe->frames=nf;
		nf->t=&ErrorHandlerContextMethodTable;
		EhFrameContext* c=(EhFrameContext*)nf->data;
		c->num=errSymbolLists.top;
		FklVMvalue** t=(FklVMvalue**)fklRealloc(errSymbolLists.base,errSymbolLists.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorSymbolLists=t;
		t=(FklVMvalue**)fklRealloc(errHandlers.base,errHandlers.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorHandlers=t;
		c->tp=exe->tp;
		fklCallObj(exe,proc);
	}
	else
	{
		fklUninitPtrStack(&errSymbolLists);
		fklUninitPtrStack(&errHandlers);
		fklTailCallObj(exe,proc);
	}
	fklSetBp(exe);
#undef GET_PROC
#undef GET_LIST
	return 1;
}

static int builtin_apply(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.apply";
	DECL_AND_CHECK_ARG(proc,Pname);
	FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
	FklPtrStack stack1=FKL_STACK_INIT;
	fklInitPtrStack(&stack1,32,16);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=FKL_VM_POP_ARG(exe)))
	{
		if(!fklResBp(exe))
		{
			lastList=value;
			break;
		}
		fklPushPtrStack(value,&stack1);
	}
	if(!lastList)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
	FklPtrStack stack2=FKL_STACK_INIT;
	fklInitPtrStack(&stack2,32,16);
	if(!FKL_IS_PAIR(lastList)&&lastList!=FKL_VM_NIL)
	{
		fklUninitPtrStack(&stack1);
		fklUninitPtrStack(&stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=FKL_VM_CDR(lastList))
		fklPushPtrStack(FKL_VM_CAR(lastList),&stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklUninitPtrStack(&stack1);
		fklUninitPtrStack(&stack2);
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	fklSetBp(exe);
	while(!fklIsPtrStackEmpty(&stack2))
	{
		FklVMvalue* t=fklPopPtrStack(&stack2);
		FKL_VM_PUSH_VALUE(exe,t);
	}
	fklUninitPtrStack(&stack2);
	while(!fklIsPtrStackEmpty(&stack1))
	{
		FklVMvalue* t=fklPopPtrStack(&stack1);
		FKL_VM_PUSH_VALUE(exe,t);
	}
	fklUninitPtrStack(&stack1);
	fklTailCallObj(exe,proc);
	return 1;
}

static int builtin_map(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.map";
	switch(ctx->context)
	{
		case 0:
			{
				DECL_AND_CHECK_ARG(proc,Pname);
				FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
				size_t arg_num=exe->tp-exe->bp;
				if(arg_num==0)
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
				FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);
				uint32_t rtp=fklResBpIn(exe,arg_num);
				FKL_CHECK_TYPE(base[0],fklIsList,Pname,exe);
				size_t len=fklVMlistLength(base[0]);
				for(size_t i=1;i<arg_num;i++)
				{
					FKL_CHECK_TYPE(base[i],fklIsList,Pname,exe);
					if(fklVMlistLength(base[i])!=len)
						FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_LIST_DIFFER_IN_LENGTH,exe);
				}
				if(len==0)
				{
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
					return 0;
				}

				FklVMvalue* resultBox=fklCreateVMvalueBox(exe,FKL_VM_NIL);
				ctx->context=(uintptr_t)&FKL_VM_BOX(resultBox);
				fklCprocRestituteSframe(exe,rtp);
				FKL_VM_GET_VALUE(exe,arg_num+1)=resultBox;
				FKL_VM_PUSH_VALUE(exe,proc);
				fklSetBp(exe);
				for(FklVMvalue* const* const end=&base[arg_num];base<end;base++)
				{
					FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(*base));
					*base=FKL_VM_CDR(*base);
				}
				fklCallObj(exe,proc);
				return 1;
			}
			break;
		default:
			{
				FklVMvalue** cur=(FklVMvalue**)(ctx->context);
				FklVMvalue* r=FKL_VM_POP_TOP_VALUE(exe);
				FklVMvalue* pair=fklCreateVMvaluePairWithCar(exe,r);
				*cur=pair;
				ctx->context=(uintptr_t)&FKL_VM_CDR(pair);
				size_t arg_num=exe->tp-ctx->rtp;
				FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num-1);
				if(base[0]==FKL_VM_NIL)
					fklSetTpAndPushValue(exe
							,ctx->rtp
							,FKL_VM_BOX(FKL_VM_GET_VALUE(exe,arg_num)));
				else
				{
					FklVMvalue* proc=FKL_VM_GET_VALUE(exe,1);
					fklSetBp(exe);
					for(FklVMvalue* const* const end=&base[arg_num-2];base<end;base++)
					{
						FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(*base));
						*base=FKL_VM_CDR(*base);
					}
					fklCallObj(exe,proc);
					return 1;
				}
			}
			break;
	}
	return 0;
}

#define AND_OR_MAP_PATTERN(NAME,DEFAULT,EXIT) static const char Pname[]=NAME;\
	switch(ctx->context)\
	{\
		case 0:\
			{\
				DECL_AND_CHECK_ARG(proc,Pname);\
				FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);\
				size_t arg_num=exe->tp-exe->bp;\
				if(arg_num==0)\
					FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);\
				FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);\
				uint32_t rtp=fklResBpIn(exe,arg_num);\
				fklResBp(exe);\
				FKL_CHECK_TYPE(base[0],fklIsList,Pname,exe);\
				size_t len=fklVMlistLength(base[0]);\
				for(size_t i=1;i<arg_num;i++)\
				{\
					FKL_CHECK_TYPE(base[i],fklIsList,Pname,exe);\
					if(fklVMlistLength(base[i])!=len)\
						FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_LIST_DIFFER_IN_LENGTH,exe);\
				}\
				if(len==0)\
				{\
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,DEFAULT);\
					return 0;\
				}\
				ctx->context=1;\
				fklCprocRestituteSframe(exe,rtp);\
				FKL_VM_GET_VALUE(exe,arg_num+1)=FKL_VM_NIL;\
				FKL_VM_PUSH_VALUE(exe,proc);\
				fklSetBp(exe);\
				for(FklVMvalue* const* const end=&base[arg_num];base<end;base++)\
				{\
					FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(*base));\
					*base=FKL_VM_CDR(*base);\
				}\
				fklCallObj(exe,proc);\
				return 1;\
			}\
			break;\
		case 1:\
			{\
				FklVMvalue* r=FKL_VM_POP_TOP_VALUE(exe);\
				EXIT;\
				size_t arg_num=exe->tp-ctx->rtp;\
				FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num-1);\
				if(base[0]==FKL_VM_NIL)\
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe\
							,ctx->rtp\
							,r);\
				else\
				{\
					FklVMvalue* proc=FKL_VM_GET_VALUE(exe,1);\
					fklSetBp(exe);\
					for(FklVMvalue* const* const end=&base[arg_num-2];base<end;base++)\
					{\
						FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(*base));\
						*base=FKL_VM_CDR(*base);\
					}\
					fklCallObj(exe,proc);\
					return 1;\
				}\
			}\
			break;\
	}\
	return 0;

static int builtin_foreach(FKL_CPROC_ARGL)
{
	AND_OR_MAP_PATTERN("builtin.foreach",FKL_VM_NIL,);
}

static int builtin_andmap(FKL_CPROC_ARGL)
{
	AND_OR_MAP_PATTERN("builtin.andmap"
			,FKL_VM_TRUE
			,if(r==FKL_VM_NIL)
			{
			FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,r);
			return 0;
			});
}

static int builtin_ormap(FKL_CPROC_ARGL)
{
	AND_OR_MAP_PATTERN("builtin.ormap"
			,FKL_VM_NIL
			,if(r!=FKL_VM_NIL)
			{
			FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,r);
			return 0;
			});
}

#undef AND_OR_MAP_PATTERN

static int builtin_memq(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.memq";
	DECL_AND_CHECK_ARG2(obj,list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	FklVMvalue* r=list;
	for(;r!=FKL_VM_NIL;r=FKL_VM_CDR(r))
		if(FKL_VM_CAR(r)==obj)
			break;
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_member(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.member";
	switch(ctx->context)
	{
		case 0:
			{
				DECL_AND_CHECK_ARG2(obj,list,Pname);
				FklVMvalue* proc=FKL_VM_POP_ARG(exe);
				FKL_CHECK_REST_ARG(exe,Pname);
				FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
				if(proc)
				{
					FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
					if(list==FKL_VM_NIL)
					{
						FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
						return 0;
					}
					ctx->context=1;
					fklCprocRestituteSframe(exe,exe->tp);
					FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
					FKL_VM_PUSH_VALUE(exe,proc);
					FKL_VM_PUSH_VALUE(exe,obj);
					FKL_VM_PUSH_VALUE(exe,list);
					fklSetBp(exe);
					FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
					FKL_VM_PUSH_VALUE(exe,obj);
					fklCallObj(exe,proc);
					return 1;
				}
				FklVMvalue* r=list;
				for(;r!=FKL_VM_NIL;r=FKL_VM_CDR(r))
					if(fklVMvalueEqual(FKL_VM_CAR(r),obj))
						break;
				FKL_VM_PUSH_VALUE(exe,r);
			}
			break;
		case 1:
			{
				FklVMvalue* r=FKL_VM_POP_TOP_VALUE(exe);
				FklVMvalue** plist=&FKL_VM_GET_VALUE(exe,1);
				FklVMvalue* list=*plist;
				if(r==FKL_VM_NIL)
					list=FKL_VM_CDR(list);
				else
				{
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,list);
					return 0;
				}
				if(list==FKL_VM_NIL)
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,FKL_VM_NIL);
				else
				{
					*plist=list;
					FklVMvalue* obj=FKL_VM_GET_VALUE(exe,2);
					FklVMvalue* proc=FKL_VM_GET_VALUE(exe,3);
					fklSetBp(exe);
					FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
					FKL_VM_PUSH_VALUE(exe,obj);
					fklCallObj(exe,proc);
					return 1;
				}
			}
			break;
	}
	return 0;
}

static int builtin_memp(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.memp";
	switch(ctx->context)
	{
		case 0:
			{
				DECL_AND_CHECK_ARG2(proc,list,Pname);
				FKL_CHECK_REST_ARG(exe,Pname);
				FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
				FKL_CHECK_TYPE(list,fklIsList,Pname,exe);

				if(list==FKL_VM_NIL)
				{
					FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
					return 0;
				}
				ctx->context=1;
				fklCprocRestituteSframe(exe,exe->tp);
				FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
				FKL_VM_PUSH_VALUE(exe,proc);
				FKL_VM_PUSH_VALUE(exe,list);
				fklSetBp(exe);
				FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
				fklCallObj(exe,proc);
				return 1;
			}
			break;
		case 1:
			{
				FklVMvalue* r=FKL_VM_POP_TOP_VALUE(exe);
				FklVMvalue** plist=&FKL_VM_GET_VALUE(exe,1);

				FklVMvalue* list=*plist;
				if(r==FKL_VM_NIL)
					list=FKL_VM_CDR(list);
				else
				{
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,list);
					return 0;
				}
				if(list==FKL_VM_NIL)
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,FKL_VM_NIL);
				else
				{
					*plist=list;
					FklVMvalue* proc=FKL_VM_GET_VALUE(exe,2);
					fklSetBp(exe);
					FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
					fklCallObj(exe,proc);
					return 1;
				}
			}
			break;
	}
	return 0;
}

static int builtin_filter(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.filter";
	switch(ctx->context)
	{
		case 0:
			{
				DECL_AND_CHECK_ARG2(proc,list,Pname);
				FKL_CHECK_REST_ARG(exe,Pname);
				FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
				FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
				if(list==FKL_VM_NIL)
				{
					FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
					return 0;
				}
				FklVMvalue* resultBox=fklCreateVMvalueBoxNil(exe);
				ctx->context=(uintptr_t)&FKL_VM_BOX(resultBox);
				fklCprocRestituteSframe(exe,exe->tp);
				FKL_VM_PUSH_VALUE(exe,resultBox);
				FKL_VM_PUSH_VALUE(exe,proc);
				FKL_VM_PUSH_VALUE(exe,list);
				fklSetBp(exe);
				FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
				fklCallObj(exe,proc);
				return 1;
			}
			break;
		default:
			{
				FklVMvalue** cur=(FklVMvalue**)(ctx->context);
				FklVMvalue* r=FKL_VM_POP_TOP_VALUE(exe);
				FklVMvalue** plist=&FKL_VM_GET_VALUE(exe,1);
				FklVMvalue* list=*plist;
				if(r!=FKL_VM_NIL)
				{
					FklVMvalue* pair=fklCreateVMvaluePairWithCar(exe,FKL_VM_CAR(list));
					*cur=pair;
					ctx->context=(uintptr_t)&FKL_VM_CDR(pair);
				}
				list=FKL_VM_CDR(list);
				if(list==FKL_VM_NIL)
					fklSetTpAndPushValue(exe
							,ctx->rtp
							,FKL_VM_BOX(FKL_VM_GET_VALUE(exe,3)));
				else
				{
					*plist=list;
					FklVMvalue* proc=FKL_VM_GET_VALUE(exe,2);
					fklSetBp(exe);
					FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
					fklCallObj(exe,proc);
					return 1;
				}
			}
			break;
	}
	return 0;
}

static int builtin_list(FKL_CPROC_ARGL)
{
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pcur=&r;
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,cur);
		pcur=&FKL_VM_CDR(*pcur);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_list8(FKL_CPROC_ARGL)
{
	DECL_AND_CHECK_ARG(r,"builtin.list*");
	FklVMvalue** pcur=&r;
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		*pcur=fklCreateVMvaluePair(exe,*pcur,cur);
		pcur=&FKL_VM_CDR(*pcur);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_reverse(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.reverse";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsList,Pname,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=FKL_VM_CDR(cdr))
		retval=fklCreateVMvaluePair(exe,FKL_VM_CAR(cdr),retval);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_reverse1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.reverse!";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,fklIsList,Pname,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cdr=obj;
	while(cdr!=FKL_VM_NIL)
	{
		FklVMvalue* car=FKL_VM_CAR(cdr);
		FklVMvalue* pair=cdr;
		cdr=FKL_VM_CDR(cdr);
		FKL_VM_CAR(pair)=car;
		FKL_VM_CDR(pair)=retval;
		retval=pair;
	}
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_feof_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.feof";
	DECL_AND_CHECK_ARG(fp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(fp,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,feof(FKL_VM_FP(fp)->fp)?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int builtin_fclerr(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fclerr";
	DECL_AND_CHECK_ARG(fp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(fp,Pname,exe);
	clearerr(FKL_VM_FP(fp)->fp);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_facce_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.facce?";
	DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_freg_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.freg?";
	DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_fdir_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fdir?";
	DECL_AND_CHECK_ARG(filename,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename,FKL_IS_STR,Pname,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklIsAccessibleDirectory(FKL_VM_STR(filename)->str)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_ftell(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.ftell";
	DECL_AND_CHECK_ARG(fp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(fp,Pname,exe);
	int64_t pos=ftell(FKL_VM_FP(fp)->fp);
	if(pos<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
	FklVMvalue* r=pos>FKL_FIX_INT_MAX
		?fklCreateVMvalueBigIntWithI64(exe,pos)
		:FKL_MAKE_VM_FIX(pos);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_fseek(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fseek";
	DECL_AND_CHECK_ARG2(vfp,offset,Pname);
	FklVMvalue* whence_v=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(vfp,FKL_IS_FP,Pname,exe);
	FKL_CHECK_TYPE(offset,fklIsVMint,Pname,exe);
	if(whence_v)
		FKL_CHECK_TYPE(whence_v,FKL_IS_SYM,Pname,exe);
	CHECK_FP_OPEN(vfp,Pname,exe);
	FILE* fp=FKL_VM_FP(vfp)->fp;
	const FklSid_t seek_set=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->seek_set;
	const FklSid_t seek_cur=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->seek_cur;
	const FklSid_t seek_end=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->seek_end;
	const FklSid_t whence_sid=whence_v?FKL_GET_SYM(whence_v):seek_set;
	int whence=whence_sid==seek_set?0
		:whence_sid==seek_cur?1
		:whence_sid==seek_end?2:-1;

	if(whence==-1)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
	if(fseek(fp,fklGetInt(offset),whence))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
	else
	{
		int64_t pos=ftell(fp);
		FklVMvalue* r=pos>FKL_FIX_INT_MAX
			?fklCreateVMvalueBigIntWithI64(exe,pos)
			:FKL_MAKE_VM_FIX(pos);
		FKL_VM_PUSH_VALUE(exe,r);
	}
	return 0;
}

static int builtin_fflush(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fflush";
	FklVMvalue* fp=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(fp)
	{
		FKL_CHECK_TYPE(fp,FKL_IS_FP,Pname,exe);
		CHECK_FP_OPEN(fp,Pname,exe);
		if(fflush(FKL_VM_FP(fp)->fp))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
	}
	else if(fflush(NULL))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_vector(FKL_CPROC_ARGL)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(size_t i=0;i<size;i++)
		v->base[i]=FKL_VM_POP_ARG(exe);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_getcwd(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.getcwd";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* s=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(fklGetCwd()));
	FKL_VM_PUSH_VALUE(exe,s);
	return 0;
}

static int builtin_chdir(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.chdir";
	DECL_AND_CHECK_ARG(dir,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(dir,FKL_IS_STR,Pname,exe);
	FklString* dirstr=FKL_VM_STR(dir);
	int r=fklChangeWorkDir(dirstr->str);
	if(r)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,dirstr->str,0,FKL_ERR_FILEFAILURE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_fwrite(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.fwrite";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);

	GET_OR_USE_STDOUT(file);
	FKL_CHECK_TYPE(file,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);
	FILE* fp=FKL_VM_FP(file)->fp;
	if(FKL_IS_STR(obj))
	{
		FklString* str=FKL_VM_STR(obj);
		fwrite(str->str,str->size,1,fp);
	}
	if(FKL_IS_BYTEVECTOR(obj))
	{
		FklBytevector* bvec=FKL_VM_BVEC(obj);
		fwrite(bvec->ptr,bvec->size,1,fp);
	}
	else if(FKL_IS_USERDATA(obj)&&fklIsWritableUd(FKL_VM_UD(obj)))
		fklWriteVMudata(FKL_VM_UD(obj),fp);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_box(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.box";
	DECL_AND_SET_DEFAULT(obj,FKL_VM_NIL);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,obj));
	return 0;
}

static int builtin_unbox(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.unbox";
	DECL_AND_CHECK_ARG(box,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_BOX(box));
	return 0;
}

static int builtin_box_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.box-set!";
	DECL_AND_CHECK_ARG2(box,obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,Pname,exe);
	FKL_VM_BOX(box)=obj;
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_box_cas(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.box-cas!";
	DECL_AND_CHECK_ARG3(box,old,new,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,Pname,exe);
	if(FKL_VM_BOX(box)==old)
	{
		FKL_VM_BOX(box)=new;
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_bytevector(FKL_CPROC_ARGL)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(size,NULL));
	FklBytevector* bytevec=FKL_VM_BVEC(r);
	size_t i=0;
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur!=NULL
			;cur=FKL_VM_POP_ARG(exe),i++)
	{
		FKL_CHECK_TYPE(cur,fklIsVMint,"builtin.bytevector",exe);
		bytevec->ptr[i]=fklGetInt(cur);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_bytevector(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-bytevector";
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsVMint,Pname,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(len,NULL));
	FklBytevector* bytevec=FKL_VM_BVEC(r);
	uint8_t u_8=0;
	if(content)
	{
		FKL_CHECK_TYPE(content,fklIsVMint,Pname,exe);
		u_8=fklGetInt(content);
	}
	memset(bytevec->ptr,u_8,len);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

#define PREDICATE(condtion,err_infor) DECL_AND_CHECK_ARG(val,err_infor);\
	FKL_CHECK_REST_ARG(exe,err_infor);\
	if(condtion)\
	FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);\
	else\
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);\
	return 0;

static int builtin_sleep(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.sleep";
	DECL_AND_CHECK_ARG(second,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(second,fklIsVMint,Pname,exe);
	if(fklVMnumberLt0(second))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t sec=fklGetUint(second);
	fklVMsleep(exe,sec*1000);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_msleep(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.msleep";
	DECL_AND_CHECK_ARG(second,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(second,fklIsVMint,Pname,exe);
	if(fklVMnumberLt0(second))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t ms=fklGetUint(second);
	fklVMsleep(exe,ms);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_srand(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.srand";
	FklVMvalue* s=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(s,fklIsVMint,Pname,exe);
	srand(fklGetInt(s));
	FKL_VM_PUSH_VALUE(exe,s);
	return 0;
}

static int builtin_rand(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.rand";
	FklVMvalue*  lim=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	if(lim&&!fklIsVMint(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))));
	return 0;
}

#include<time.h>

static int builtin_get_time(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.get-time";
	FKL_CHECK_REST_ARG(exe,Pname);
	time_t timer=time(NULL);
	const struct tm* tblock=localtime(&timer);

	FklStringBuffer buf;
	fklInitStringBuffer(&buf);

	fklStringBufferPrintf(&buf,"%u-%u-%u_%u_%u_%u"
			,tblock->tm_year+1900
			,tblock->tm_mon+1
			,tblock->tm_mday
			,tblock->tm_hour
			,tblock->tm_min
			,tblock->tm_sec);

	FklVMvalue* tmpVMvalue=fklCreateVMvalueStr(exe,fklCreateString(buf.index,buf.buf));
	fklUninitStringBuffer(&buf);

	FKL_VM_PUSH_VALUE(exe,tmpVMvalue);
	return 0;
}

static int builtin_fremove(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.remove-file";
	DECL_AND_CHECK_ARG(name,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	FklString* nameStr=FKL_VM_STR(name);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(remove(nameStr->str)));
	return 0;
}

static int builtin_time(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.time";
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint((int64_t)time(NULL),exe));
	return 0;
}

static int builtin_system(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.system";
	DECL_AND_CHECK_ARG(name,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	FklString* nameStr=FKL_VM_STR(name);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMint(system(nameStr->str),exe));
	return 0;
}

static int builtin_hash(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash";
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_hash_num(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-num";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(FKL_VM_HASH(ht)->num,exe));
	return 0;
}

static int builtin_make_hash(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-hash";
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		FklVMvalue* value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_hasheqv(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hasheqv";
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_make_hasheqv(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-hasheqv";
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		FklVMvalue* value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_hashequal(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hashequal";
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_make_hashequal(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.make-hashequal";
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		FklVMvalue* value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_hash_ref(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-ref";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FklVMvalue* defa=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	int ok=0;
	FklVMvalue* retval=fklVMhashTableGet(key,FKL_VM_HASH(ht),&ok);
	if(ok)
		FKL_VM_PUSH_VALUE(exe,retval);
	else
	{
		if(defa)
			FKL_VM_PUSH_VALUE(exe,defa);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NO_VALUE_FOR_KEY,exe);
	}
	return 0;
}

static int builtin_hash_ref4(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-ref$";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMhashTableItem* i=fklVMhashTableGetItem(key,FKL_VM_HASH(ht));
	if(i)
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,i->key,i->v));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_hash_ref7(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-ref&";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	int ok=0;
	FklVMvalue* retval=fklVMhashTableGet(key,FKL_VM_HASH(ht),&ok);
	if(ok)
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,retval));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_hash_ref1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-ref!";
	DECL_AND_CHECK_ARG3(ht,key,toSet,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMhashTableItem* item=fklVMhashTableRef1(key,toSet,FKL_VM_HASH(ht),exe->gc);
	FKL_VM_PUSH_VALUE(exe,item->v);
	return 0;
}

static int builtin_hash_clear(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-clear!";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	fklClearHashTable(FKL_VM_HASH(ht));
	FKL_VM_PUSH_VALUE(exe,ht);
	return 0;
}

static int builtin_hash_set(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-set!";
	DECL_AND_CHECK_ARG3(ht,key,value,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	fklVMhashTableSet(key,value,FKL_VM_HASH(ht));
	FKL_VM_PUSH_VALUE(exe,value);;
	return 0;
}

static int builtin_hash_del1(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-del!";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMhashTableItem i;
	if(fklDelHashItem(&key,FKL_VM_HASH(ht),&i))
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,i.key,i.v));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_hash_set8(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-set*!";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMvalue* value=NULL;
	FklHashTable* hash=FKL_VM_HASH(ht);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,hash);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,value);
	return 0;
}

static int builtin_hash_to_list(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash->list";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklHashTable* hash=FKL_VM_HASH(ht);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		FklVMvalue* pair=fklCreateVMvaluePair(exe,item->key,item->v);
		*cur=fklCreateVMvaluePairWithCar(exe,pair);
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_hash_keys(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-keys";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklHashTable* hash=FKL_VM_HASH(ht);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		*cur=fklCreateVMvaluePairWithCar(exe,item->key);
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_hash_values(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.hash-values";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklHashTable* hash=FKL_VM_HASH(ht);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		*cur=fklCreateVMvaluePairWithCar(exe,item->v);
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_not(FKL_CPROC_ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.not")}
static int builtin_null(FKL_CPROC_ARGL) {PREDICATE(val==FKL_VM_NIL,"builtin.null")}
static int builtin_atom(FKL_CPROC_ARGL) {PREDICATE(!FKL_IS_PAIR(val),"builtin.atom?")}
static int builtin_char_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_CHR(val),"builtin.char?")}
static int builtin_integer_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_FIX(val)||FKL_IS_BIG_INT(val),"builtin.integer?")}
static int builtin_fix_int_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_FIX(val),"builtin.fix-int?")}
static int builtin_f64_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_F64(val),"builtin.i64?")}
static int builtin_number_p(FKL_CPROC_ARGL) {PREDICATE(fklIsVMnumber(val),"builtin.number?")}
static int builtin_pair_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_PAIR(val),"builtin.pair?")}
static int builtin_symbol_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_SYM(val),"builtin.symbol?")}
static int builtin_string_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_STR(val),"builtin.string?")}
static int builtin_error_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_ERR(val),"builtin.error?")}
static int builtin_procedure_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_PROC(val)||FKL_IS_CPROC(val),"builtin.procedure?")}
static int builtin_proc_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_PROC(val),"builtin.proc?")}
static int builtin_cproc_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_CPROC(val),"builtin.cproc?")}
static int builtin_vector_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_VECTOR(val),"builtin.vector?")}
static int builtin_bytevector_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_BYTEVECTOR(val),"builtin.bytevector?")}
static int builtin_chanl_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_CHAN(val),"builtin.chanl?")}
static int builtin_dll_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_DLL(val),"builtin.dll?")}
static int builtin_big_int_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_BIG_INT(val),"builtin.big-int?")}
static int builtin_list_p(FKL_CPROC_ARGL){PREDICATE(fklIsList(val),"builtin.list?")}
static int builtin_box_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_BOX(val),"builtin.box?")}
static int builtin_hash_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE(val),"builtin.hash?")}
static int builtin_hasheq_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQ(val),"builtin.hash?")}
static int builtin_hasheqv_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQV(val),"builtin.hash?")}
static int builtin_hashequal_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQUAL(val),"builtin.hash?")}

static int builtin_eof_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_USERDATA(val)&&FKL_VM_UD(val)->t==&EofUserDataMetaTable,"builtin.eof?")}

static int builtin_parser_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_USERDATA(val)&&FKL_VM_UD(val)->t==&CustomParserMetaTable,"builtin.parser?")}

static int builtin_odd_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.odd?";
	DECL_AND_CHECK_ARG(val,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(val,fklIsVMint,Pname,exe);
	int r=0;
	if(FKL_IS_FIX(val))
		r=FKL_GET_FIX(val)%2;
	else
		r=fklIsBigIntOdd(FKL_VM_BI(val));
	if(r)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_even_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.even?";
	DECL_AND_CHECK_ARG(val,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(val,fklIsVMint,Pname,exe);
	int r=0;
	if(FKL_IS_FIX(val))
		r=FKL_GET_FIX(val)%2==0;
	else
		r=fklIsBigIntEven(FKL_VM_BI(val));
	if(r)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_exit(FKL_CPROC_ARGL)
{
	static const char Pname[]="builtin.exit";
	FklVMvalue* exit_no=FKL_VM_POP_ARG(exe);
	if(exit_no)
		FKL_CHECK_TYPE(exit_no,FKL_IS_FIX,Pname,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	exe->state=FKL_VM_EXIT;
	if(exe->chan==NULL)
		exe->gc->exit_code=exit_no?FKL_GET_FIX(exit_no):0;
	FKL_VM_PUSH_VALUE(exe,exit_no?FKL_MAKE_VM_FIX(0):exit_no);
	return 0;
}

#undef PREDICATE
//end

#include<fakeLisp/opcode.h>

#define INL_FUNC_ARGS FklByteCodelnt* bcs[],FklSid_t fid,uint64_t line

static inline FklByteCodelnt* inl_0_arg_func(FklOpcode opc,FklSid_t fid,uint64_t line)
{
	FklInstruction ins={.op=opc};
	return fklCreateSingleInsBclnt(ins,fid,line);
}

static inline FklByteCodelnt* inlfunc_box0(INL_FUNC_ARGS)
{
	return inl_0_arg_func(FKL_OP_BOX0,fid,line);
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
	FklInstruction bc={.op=opc};
	fklBytecodeLntPushFrontIns(bcs[0],&bc,fid,line);
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
	FklInstruction bc={.op=opc};
	fklCodeLntReverseConcat(bcs[1],bcs[0]);
	fklBytecodeLntPushFrontIns(bcs[0],&bc,fid,line);
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

static FklByteCodelnt* inlfunc_vec_ref(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_VEC_REF,bcs,fid,line);
}

static FklByteCodelnt* inlfunc_str_ref(INL_FUNC_ARGS)
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
	FklInstruction bc={.op=opc};
	fklCodeLntReverseConcat(bcs[1],bcs[0]);
	fklCodeLntReverseConcat(bcs[2],bcs[0]);
	fklBytecodeLntPushFrontIns(bcs[0],&bc,fid,line);
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
	FklVMcFunc f;
	FklBuiltinInlineFunc inlfunc[4];
}builtInSymbolList[]=
{
	{"stdin",                 NULL,                            {NULL,         NULL,          NULL,            NULL,          }, },
	{"stdout",                NULL,                            {NULL,         NULL,          NULL,            NULL,          }, },
	{"stderr",                NULL,                            {NULL,         NULL,          NULL,            NULL,          }, },
	{"car",                   builtin_car,                     {NULL,         inlfunc_car,   NULL,            NULL,          }, },
	{"cdr",                   builtin_cdr,                     {NULL,         inlfunc_cdr,   NULL,            NULL,          }, },
	{"cons",                  builtin_cons,                    {NULL,         NULL,          inlfunc_cons,    NULL,          }, },
	{"append",                builtin_append,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"append!",               builtin_append1,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"copy",                  builtin_copy,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"atom",                  builtin_atom,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"null",                  builtin_null,                    {NULL,         inlfunc_not,   NULL,            NULL,          }, },
	{"not",                   builtin_not,                     {NULL,         inlfunc_not,   NULL,            NULL,          }, },
	{"eq",                    builtin_eq,                      {NULL,         NULL,          inlfunc_eq,      NULL,          }, },
	{"eqv",                   builtin_eqv,                     {NULL,         NULL,          inlfunc_eqv,     NULL,          }, },
	{"equal",                 builtin_equal,                   {NULL,         NULL,          inlfunc_equal,   NULL,          }, },
	{"=",                     builtin_eqn,                     {NULL,         inlfunc_true,  inlfunc_eqn,     inlfunc_eqn3,  }, },
	{"+",                     builtin_add,                     {inlfunc_add0, inlfunc_add1,  inlfunc_add,     inlfunc_add3,  }, },
	{"1+",                    builtin_add_1,                   {NULL,         inlfunc_add_1, NULL,            NULL,          }, },
	{"-",                     builtin_sub,                     {NULL,         inlfunc_neg,   inlfunc_sub,     inlfunc_sub3,  }, },
	{"-1+",                   builtin_sub_1,                   {NULL,         inlfunc_sub_1, NULL,            NULL,          }, },
	{"*",                     builtin_mul,                     {inlfunc_mul0, inlfunc_mul1,  inlfunc_mul,     inlfunc_mul3,  }, },
	{"/",                     builtin_div,                     {NULL,         inlfunc_rec,   inlfunc_div,     inlfunc_div3,  }, },
	{"//",                    builtin_idiv,                    {NULL,         NULL,          inlfunc_idiv,    inlfunc_idiv3, }, },
	{"%",                     builtin_mod,                     {NULL,         NULL,          inlfunc_mod,     NULL,          }, },
	{"abs",                   builtin_abs,                     {NULL,         NULL,          NULL,            NULL,          }, },
	{">",                     builtin_gt,                      {NULL,         inlfunc_true,  inlfunc_gt,      inlfunc_gt3,   }, },
	{">=",                    builtin_ge,                      {NULL,         inlfunc_true,  inlfunc_ge,      inlfunc_ge3,   }, },
	{"<",                     builtin_lt,                      {NULL,         inlfunc_true,  inlfunc_lt,      inlfunc_lt3,   }, },
	{"<=",                    builtin_le,                      {NULL,         inlfunc_true,  inlfunc_le,      inlfunc_le3,   }, },
	{"nth",                   builtin_nth,                     {NULL,         NULL,          inlfunc_nth,     NULL,          }, },
	{"length",                builtin_length,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"apply",                 builtin_apply,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"call/eh",               builtin_call_eh,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"read",                  builtin_read,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"parse",                 builtin_parse,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-parser",           builtin_make_parser,             {NULL,         NULL,          NULL,            NULL,          }, },
	{"parser?",               builtin_parser_p,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"stringify",             builtin_stringify,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"prin1",                 builtin_prin1,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"princ",                 builtin_princ,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"println",               builtin_println,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"print",                 builtin_print,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"fprint",                builtin_fprint,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"fprin1",                builtin_fprin1,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"newline",               builtin_newline,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"dlopen",                builtin_dlopen,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"dlsym",                 builtin_dlsym,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"argv",                  builtin_argv,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"go",                    builtin_go,                      {NULL,         NULL,          NULL,            NULL,          }, },
	{"chanl",                 builtin_chanl,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"chanl#msg",             builtin_chanl_msg_num,           {NULL,         NULL,          NULL,            NULL,          }, },
	{"chanl#recv",            builtin_chanl_recv_num,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"chanl#send",            builtin_chanl_send_num,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"chanl-full?",           builtin_chanl_full_p,            {NULL,         NULL,          NULL,            NULL,          }, },
	{"chanl-msg->list",       builtin_chanl_msg_to_list,       {NULL,         NULL,          NULL,            NULL,          }, },
	{"send",                  builtin_send,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"recv",                  builtin_recv,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"recv&",                 builtin_recv7,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"error",                 builtin_error,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"error-type",            builtin_error_type,              {NULL,         NULL,          NULL,            NULL,          }, },
	{"error-where",           builtin_error_where,             {NULL,         NULL,          NULL,            NULL,          }, },
	{"error-msg",             builtin_error_msg,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"raise",                 builtin_raise,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"throw",                 builtin_throw,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"reverse",               builtin_reverse,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"reverse!",              builtin_reverse1,                {NULL,         NULL,          NULL,            NULL,          }, },

	{"nthcdr",                builtin_nthcdr,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"tail",                  builtin_tail,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"char?",                 builtin_char_p,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"integer?",              builtin_integer_p,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"fix-int?",              builtin_fix_int_p,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"big-int?",              builtin_big_int_p,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"f64?",                  builtin_f64_p,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"pair?",                 builtin_pair_p,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"symbol?",               builtin_symbol_p,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"string->symbol",        builtin_string_to_symbol,        {NULL,         NULL,          NULL,            NULL,          }, },

	{"string?",               builtin_string_p,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"string",                builtin_string,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"substring",             builtin_substring,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"sub-string",            builtin_sub_string,              {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-string",           builtin_make_string,             {NULL,         NULL,          NULL,            NULL,          }, },
	{"symbol->string",        builtin_symbol_to_string,        {NULL,         NULL,          NULL,            NULL,          }, },
	{"number->string",        builtin_number_to_string,        {NULL,         NULL,          NULL,            NULL,          }, },
	{"integer->string",       builtin_integer_to_string,       {NULL,         NULL,          NULL,            NULL,          }, },
	{"f64->string",           builtin_f64_to_string,           {NULL,         NULL,          NULL,            NULL,          }, },
	{"vector->string",        builtin_vector_to_string,        {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector->string",    builtin_bytevector_to_string,    {NULL,         NULL,          NULL,            NULL,          }, },
	{"list->string",          builtin_list_to_string,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"->string",              builtin_to_string,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"str-ref",               builtin_str_ref,                 {NULL,         NULL,          inlfunc_str_ref, NULL,          }, },
	{"str-set!",              builtin_str_set1,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"string-fill!",          builtin_string_fill,             {NULL,         NULL,          NULL,            NULL,          }, },

	{"error?",                builtin_error_p,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"procedure?",            builtin_procedure_p,             {NULL,         NULL,          NULL,            NULL,          }, },
	{"proc?",                 builtin_proc_p,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"cproc?",                builtin_cproc_p,                 {NULL,         NULL,          NULL,            NULL,          }, },

	{"vector?",               builtin_vector_p,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"vector",                builtin_vector,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-vector",           builtin_make_vector,             {NULL,         NULL,          NULL,            NULL,          }, },
	{"subvector",             builtin_subvector,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"sub-vector",            builtin_sub_vector,              {NULL,         NULL,          NULL,            NULL,          }, },
	{"list->vector",          builtin_list_to_vector,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"string->vector",        builtin_string_to_vector,        {NULL,         NULL,          NULL,            NULL,          }, },
	{"vec-ref",               builtin_vec_ref,                 {NULL,         NULL,          inlfunc_vec_ref, NULL,          }, },
	{"vec-set!",              builtin_vec_set,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"vec-cas!",              builtin_vec_cas,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"vector-fill!",          builtin_vector_fill,             {NULL,         NULL,          NULL,            NULL,          }, },

	{"list?",                 builtin_list_p,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"list",                  builtin_list,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"list*",                 builtin_list8,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-list",             builtin_make_list,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"vector->list",          builtin_vector_to_list,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"string->list",          builtin_string_to_list,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"nth-set!",              builtin_nth_set,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"nthcdr-set!",           builtin_nthcdr_set,              {NULL,         NULL,          NULL,            NULL,          }, },

	{"bytevector?",           builtin_bytevector_p,            {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector",            builtin_bytevector,              {NULL,         NULL,          NULL,            NULL,          }, },
	{"subbytevector",         builtin_subbytevector,           {NULL,         NULL,          NULL,            NULL,          }, },
	{"sub-bytevector",        builtin_sub_bytevector,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-bytevector",       builtin_make_bytevector,         {NULL,         NULL,          NULL,            NULL,          }, },
	{"string->bytevector",    builtin_string_to_bytevector,    {NULL,         NULL,          NULL,            NULL,          }, },
	{"vector->bytevector",    builtin_vector_to_bytevector,    {NULL,         NULL,          NULL,            NULL,          }, },
	{"list->bytevector",      builtin_list_to_bytevector,      {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector->s8-list",   builtin_bytevector_to_s8_list,   {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector->u8-list",   builtin_bytevector_to_u8_list,   {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector->s8-vector", builtin_bytevector_to_s8_vector, {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector->u8-vector", builtin_bytevector_to_u8_vector, {NULL,         NULL,          NULL,            NULL,          }, },

	{"bvs8ref",               builtin_bvs8ref,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvs16ref",              builtin_bvs16ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvs32ref",              builtin_bvs32ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvs64ref",              builtin_bvs64ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu8ref",               builtin_bvu8ref,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu16ref",              builtin_bvu16ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu32ref",              builtin_bvu32ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu64ref",              builtin_bvu64ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvf32ref",              builtin_bvf32ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvf64ref",              builtin_bvf64ref,                {NULL,         NULL,          NULL,            NULL,          }, },

	{"bvs8set!",              builtin_bvs8set1,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvs16set!",             builtin_bvs16set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvs32set!",             builtin_bvs32set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvs64set!",             builtin_bvs64set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu8set!",              builtin_bvu8set1,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu16set!",             builtin_bvu16set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu32set!",             builtin_bvu32set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvu64set!",             builtin_bvu64set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvf32set!",             builtin_bvf32set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bvf64set!",             builtin_bvf64set1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"bytevector-fill!",      builtin_bytevector_fill,         {NULL,         NULL,          NULL,            NULL,          }, },

	{"chanl?",                builtin_chanl_p,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"dll?",                  builtin_dll_p,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"getcwd",                builtin_getcwd,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"chdir",                 builtin_chdir,                   {NULL,         NULL,          NULL,            NULL,          }, },

	{"fwrite",                builtin_fwrite,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"fgets",                 builtin_fgets,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"fgetb",                 builtin_fgetb,                   {NULL,         NULL,          NULL,            NULL,          }, },

	{"fgetd",                 builtin_fgetd,                   {NULL,         NULL,          NULL,            NULL,          }, },

	{"fgetc",                 builtin_fgetc,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"fgeti",                 builtin_fgeti,                   {NULL,         NULL,          NULL,            NULL,          }, },

	{"fremove",               builtin_fremove,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"fopen",                 builtin_fopen,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"freopen",               builtin_freopen,                 {NULL,         NULL,          NULL,            NULL,          }, },

	{"freg?",                 builtin_freg_p,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"fdir?",                 builtin_fdir_p,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"fclose",                builtin_fclose,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"feof?",                 builtin_feof_p,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"eof?",                  builtin_eof_p,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"facce?",                builtin_facce_p,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"ftell",                 builtin_ftell,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"fseek",                 builtin_fseek,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"fclerr",                builtin_fclerr,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"fflush",                builtin_fflush,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"car-set!",              builtin_car_set,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"cdr-set!",              builtin_cdr_set,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"box",                   builtin_box,                     {inlfunc_box0, inlfunc_box,   NULL,            NULL,          }, },
	{"unbox",                 builtin_unbox,                   {NULL,         inlfunc_unbox, NULL,            NULL,          }, },
	{"box-set!",              builtin_box_set,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"box-cas!",              builtin_box_cas,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"box?",                  builtin_box_p,                   {NULL,         NULL,          NULL,            NULL,          }, },

	{"number?",               builtin_number_p,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"string->number",        builtin_string_to_number,        {NULL,         NULL,          NULL,            NULL,          }, },
	{"char->integer",         builtin_char_to_integer,         {NULL,         NULL,          NULL,            NULL,          }, },
	{"symbol->integer",       builtin_symbol_to_integer,       {NULL,         NULL,          NULL,            NULL,          }, },
	{"integer->char",         builtin_integer_to_char,         {NULL,         NULL,          NULL,            NULL,          }, },
	{"number->f64",           builtin_number_to_f64,           {NULL,         NULL,          NULL,            NULL,          }, },
	{"number->integer",       builtin_number_to_integer,       {NULL,         NULL,          NULL,            NULL,          }, },

	{"map",                   builtin_map,                     {NULL,         NULL,          NULL,            NULL,          }, },
	{"foreach",               builtin_foreach,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"andmap",                builtin_andmap,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"ormap",                 builtin_ormap,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"memq",                  builtin_memq,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"member",                builtin_member,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"memp",                  builtin_memp,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"filter",                builtin_filter,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"sleep",                 builtin_sleep,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"msleep",                builtin_msleep,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"srand",                 builtin_srand,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"rand",                  builtin_rand,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"get-time",              builtin_get_time,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"time",                  builtin_time,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"system",                builtin_system,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"hash",                  builtin_hash,                    {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-num",              builtin_hash_num,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-hash",             builtin_make_hash,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hasheqv",               builtin_hasheqv,                 {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-hasheqv",          builtin_make_hasheqv,            {NULL,         NULL,          NULL,            NULL,          }, },
	{"hashequal",             builtin_hashequal,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"make-hashequal",        builtin_make_hashequal,          {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash?",                 builtin_hash_p,                  {NULL,         NULL,          NULL,            NULL,          }, },
	{"hasheq?",               builtin_hasheq_p,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"hasheqv?",              builtin_hasheqv_p,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hashequal?",            builtin_hashequal_p,             {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-ref",              builtin_hash_ref,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-ref&",             builtin_hash_ref7,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-ref$",             builtin_hash_ref4,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-ref!",             builtin_hash_ref1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-set!",             builtin_hash_set,                {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-set*!",            builtin_hash_set8,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-del!",             builtin_hash_del1,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-clear!",           builtin_hash_clear,              {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash->list",            builtin_hash_to_list,            {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-keys",             builtin_hash_keys,               {NULL,         NULL,          NULL,            NULL,          }, },
	{"hash-values",           builtin_hash_values,             {NULL,         NULL,          NULL,            NULL,          }, },

	{"pmatch",                builtin_pmatch,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"odd?",                  builtin_odd_p,                   {NULL,         NULL,          NULL,            NULL,          }, },
	{"even?",                 builtin_even_p,                  {NULL,         NULL,          NULL,            NULL,          }, },

	{"exit",                  builtin_exit,                    {NULL,         NULL,          NULL,            NULL,          }, },

	{NULL,                    NULL,                            {NULL,         NULL,          NULL,            NULL,          }, },
};

static const size_t BuiltinSymbolNum=sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct)-1;

FklBuiltinInlineFunc fklGetBuiltinInlineFunc(uint32_t idx,uint32_t argNum)
{
	if(idx>=BuiltinSymbolNum)
		return NULL;
	return builtInSymbolList[idx].inlfunc[argNum];
}

uint8_t* fklGetBuiltinSymbolModifyMark(uint32_t* p)
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

uint32_t fklGetBuiltinSymbolNum(void)
{
	return BuiltinSymbolNum;
}

void fklInitGlobCodegenEnv(FklCodegenEnv* curEnv,FklSymbolTable* pst)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddCodegenBuiltinRefBySid(fklAddSymbolCstr(list->s,pst)->id,curEnv);
}

void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* table)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
		fklAddSymbolCstr(list->s,table);
}

static inline void init_vm_public_data(PublicBuiltInData* pd,FklVM* exe)
{
	FklVMvalue* builtInStdin=fklCreateVMvalueFp(exe,stdin,FKL_VM_FP_R);
	FklVMvalue* builtInStdout=fklCreateVMvalueFp(exe,stdout,FKL_VM_FP_W);
	FklVMvalue* builtInStderr=fklCreateVMvalueFp(exe,stderr,FKL_VM_FP_W);
	pd->sysIn=builtInStdin;
	pd->sysOut=builtInStdout;
	pd->sysErr=builtInStderr;

	pd->seek_set=fklAddSymbolCstr("set",exe->symbolTable)->id;
	pd->seek_cur=fklAddSymbolCstr("cur",exe->symbolTable)->id;
	pd->seek_end=fklAddSymbolCstr("end",exe->symbolTable)->id;
}

void fklInitGlobalVMclosure(FklVMframe* frame,FklVM* exe)
{
	FklVMCompoundFrameVarRef* f=&frame->c.lr;
	static const size_t RefCount=(sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct))-1;
	f->rcount=RefCount;
	FklVMvarRef** closure=(FklVMvarRef**)malloc(sizeof(FklVMvarRef*)*RefCount);
	FKL_ASSERT(closure);
	f->ref=closure;
	FklVMvalue* publicUserData=fklCreateVMvalueUdata(exe
			,&PublicBuiltInDataMetaTable
			,FKL_VM_NIL);
	FklVMudata* pud=FKL_VM_UD(publicUserData);
	FKL_DECL_UD_DATA(pd,PublicBuiltInData,pud);
	init_vm_public_data(pd,exe);

	closure[FKL_VM_STDIN_IDX]=fklCreateClosedVMvarRef(pd->sysIn);
	closure[FKL_VM_STDOUT_IDX]=fklCreateClosedVMvarRef(pd->sysOut);
	closure[FKL_VM_STDERR_IDX]=fklCreateClosedVMvarRef(pd->sysErr);

	FklSymbolTable* table=exe->symbolTable;
	for(size_t i=3;i<RefCount;i++)
	{
		FklVMvalue* v=fklCreateVMvalueCproc(exe
				,builtInSymbolList[i].f
				,NULL
				,publicUserData
				,fklAddSymbolCstr(builtInSymbolList[i].s,table)->id);
		closure[i]=fklCreateClosedVMvarRef(v);
	}
}

void fklInitGlobalVMclosureForProc(FklVMproc* proc,FklVM* exe)
{
	static const size_t RefCount=(sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct))-1;
	proc->rcount=RefCount;
	FklVMvarRef** closure=(FklVMvarRef**)malloc(sizeof(FklVMvarRef*)*proc->rcount);
	FKL_ASSERT(closure);
	proc->closure=closure;
	FklVMvalue* publicUserData=fklCreateVMvalueUdata(exe
			,&PublicBuiltInDataMetaTable
			,FKL_VM_NIL);

	FklVMudata* pud=FKL_VM_UD(publicUserData);
	FKL_DECL_UD_DATA(pd,PublicBuiltInData,pud);
	init_vm_public_data(pd,exe);

	closure[FKL_VM_STDIN_IDX]=fklCreateClosedVMvarRef(pd->sysIn);
	closure[FKL_VM_STDOUT_IDX]=fklCreateClosedVMvarRef(pd->sysOut);
	closure[FKL_VM_STDERR_IDX]=fklCreateClosedVMvarRef(pd->sysErr);

	FklSymbolTable* table=exe->symbolTable;
	for(size_t i=3;i<RefCount;i++)
	{
		FklVMvalue* v=fklCreateVMvalueCproc(exe
				,builtInSymbolList[i].f
				,NULL
				,publicUserData
				,fklAddSymbolCstr(builtInSymbolList[i].s,table)->id);
		closure[i]=fklCreateClosedVMvarRef(v);
	}
}

