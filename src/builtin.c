#include<fakeLisp/vm.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/symbol.h>
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

#define DECL_AND_CHECK_ARG(a,Pname) \
	FklVMvalue* a=fklPopArg(exe);\
	if(!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_CHECK_ARG2(a,b,Pname) \
	FklVMvalue* a=fklPopArg(exe);\
	FklVMvalue* b=fklPopArg(exe);\
	if(!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_CHECK_ARG3(a,b,c,Pname) \
	FklVMvalue* a=fklPopArg(exe);\
	FklVMvalue* b=fklPopArg(exe);\
	FklVMvalue* c=fklPopArg(exe);\
	if(!c||!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_SET_DEFAULT(a,v) \
	FklVMvalue* a=fklPopArg(exe);\
	if(!a)\
		a=v;

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

static FklVMudMetaTable PublicBuiltInDataMetaTable=
{
	.size=sizeof(PublicBuiltInData),
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
		"unsupported-operation",
		"import-missing",
	};

	for(size_t i=0;i<FKL_BUILTIN_ERR_NUM;i++)
		errorTypeId[i]=fklAddSymbolCstr(builtInErrorType[i],table)->id;
}

inline FklSid_t fklGetBuiltInErrorType(FklBuiltInErrorType type,FklSid_t errorTypeId[FKL_ERR_INCORRECT_TYPE_VALUE])
{
	return errorTypeId[type];
}

//builtin functions

#define K_FUNC_ARGL FklVM* exe,FklCCState s,void* ctx
static void builtin_car(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.car";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	fklPushVMvalue(exe,FKL_VM_CAR(obj));
}

static void builtin_car_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.car-set!";
	DECL_AND_CHECK_ARG2(obj,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	fklSetRef(&FKL_VM_CAR(obj),target,exe->gc);
	fklPushVMvalue(exe,target);
}

static void builtin_cdr(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.cdr";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	fklPushVMvalue(exe,FKL_VM_CDR(obj));
}

static void builtin_cdr_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.cdr-set!";
	DECL_AND_CHECK_ARG2(obj,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,Pname,exe);
	fklSetRef(&FKL_VM_CDR(obj),target,exe->gc);
	fklPushVMvalue(exe,target);
}

static void builtin_cons(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.cons";
	DECL_AND_CHECK_ARG2(car,cdr,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	fklPushVMvalue(exe,fklCreateVMvaluePair(exe,car,cdr));
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
	static const char Pname[]="builtin.copy";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FklVMvalue* retval=fklCopyVMvalue(obj,exe);
	if(!retval)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,retval);
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

static void builtin_append(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.append";
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklPopArg(exe);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=fklPopArg(exe);
		if(cur)
			retval=fklCopyVMvalue(retval,exe);
		if(!retval)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		for(;cur;cur=fklPopArg(exe))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	else
	{
		FklVMvalue** prev=&retval;
		for(;cur;cur=fklPopArg(exe))
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
	fklPushVMvalue(exe,retval);
}

static void builtin_append1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.append!";
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklPopArg(exe);
	if(cur&&fklIsAppendable(cur))
	{
		retval=cur;
		cur=fklPopArg(exe);
		for(;cur;cur=fklPopArg(exe))
		{
			if(valueAppend[retval->type](retval,cur))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
	}
	else
	{
		FklVMvalue** prev=&retval;
		for(;cur;cur=fklPopArg(exe))
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
	fklPushVMvalue(exe,retval);
}

static void builtin_eq(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.eq";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	fklPushVMvalue(exe
			,fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_eqv(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.eqv";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	fklPushVMvalue(exe
			,(fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_equal(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.equal";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	fklPushVMvalue(exe
			,(fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_add(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.+";
	FklVMvalue* cur=fklPopArg(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	for(;cur;cur=fklPopArg(exe))
		if(fklProcessVMnumAdd(cur,&r64,&rd,&bi))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklResBp(exe);
	fklPushVMvalue(exe,fklProcessVMnumAddResult(exe,r64,rd,&bi));
}

static void builtin_add_1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.1+";
	DECL_AND_CHECK_ARG(arg,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(arg,fklIsVMnumber,Pname,exe);
	fklPushVMvalue(exe,fklProcessVMnumInc(exe,arg));
}

static void builtin_sub(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.-";
	DECL_AND_CHECK_ARG(prev,Pname);
	FklVMvalue* cur=fklPopArg(exe);
	double rd=0.0;
	int64_t r64=0;
	FKL_CHECK_TYPE(prev,fklIsVMnumber,Pname,exe);
	if(!cur)
	{
		fklResBp(exe);
		fklPushVMvalue(exe,fklProcessVMnumNeg(exe,prev));
	}
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt0(&bi);
		for(;cur;cur=fklPopArg(exe))
			if(fklProcessVMnumAdd(cur,&r64,&rd,&bi))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklResBp(exe);
		fklPushVMvalue(exe,fklProcessVMnumSubResult(exe,prev,r64,rd,&bi));
	}
}

static void builtin_abs(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.abs";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_F64(obj))
		fklPushVMvalue(exe,fklCreateVMvalueF64(exe,fabs(FKL_VM_F64(obj))));
	else
	{
		if(FKL_IS_FIX(obj))
		{
			int64_t i=llabs(FKL_GET_FIX(obj));
			if(i>FKL_FIX_INT_MAX)
				fklPushVMvalue(exe,fklCreateVMvalueBigIntWithI64(exe,i));
			else
				fklPushVMvalue(exe,FKL_MAKE_VM_FIX(i));
		}
		else
		{
			FklVMvalue* r=fklCreateVMvalueBigInt(exe,FKL_VM_BI(obj));
			FKL_VM_BI(r)->neg=0;
			fklPushVMvalue(exe,r);
		}
	}
}

static void builtin_sub_1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.-1+";
	DECL_AND_CHECK_ARG(arg,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(arg,fklIsVMnumber,Pname,exe);
	fklPushVMvalue(exe,fklProcessVMnumDec(exe,arg));
}

static void builtin_mul(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.*";
	FklVMvalue* cur=fklPopArg(exe);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=fklPopArg(exe))
		if(fklProcessVMnumMul(cur,&r64,&rd,&bi))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklResBp(exe);
	fklPushVMvalue(exe,fklProcessVMnumMulResult(exe,r64,rd,&bi));
}

static void builtin_idiv(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.//";
	DECL_AND_CHECK_ARG2(prev,cur,Pname);
	int64_t r64=1;
	FKL_CHECK_TYPE(prev,fklIsInt,Pname,exe);
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=fklPopArg(exe))
		if(fklProcessVMintMul(cur,&r64,&bi))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(r64==0||FKL_IS_0_BIG_INT(&bi))
	{
		fklUninitBigInt(&bi);
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,fklProcessVMnumIdivResult(exe,prev,r64,&bi));
}

static void builtin_div(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin./";
	DECL_AND_CHECK_ARG(prev,Pname);
	FklVMvalue* cur=fklPopArg(exe);
	int64_t r64=1;
	double rd=1.0;
	FKL_CHECK_TYPE(prev,fklIsVMnumber,Pname,exe);
	if(!cur)
	{
		fklResBp(exe);
		FklVMvalue* r=fklProcessVMnumRec(exe,prev);
		if(!r)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
		fklPushVMvalue(exe,r);
	}
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt1(&bi);
		for(;cur;cur=fklPopArg(exe))
			if(fklProcessVMnumMul(cur,&r64,&rd,&bi))
				FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		if(r64==0||FKL_IS_0_BIG_INT(&bi)||!islessgreater(rd,0.0))
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
		}
		fklResBp(exe);
		fklPushVMvalue(exe,fklProcessVMnumDivResult(exe,prev,r64,rd,&bi));
	}
}

static void builtin_mod(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.%";
	DECL_AND_CHECK_ARG2(fir,sec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(fir,fklIsVMnumber,Pname,exe);
	FKL_CHECK_TYPE(sec,fklIsVMnumber,Pname,exe);
	FklVMvalue* r=fklProcessVMnumMod(exe,fir,sec);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_DIVZEROERROR,exe);
	fklPushVMvalue(exe,r);
}

static void builtin_eqn(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.=";
	int r=1;
	int err=0;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=fklPopArg(exe))
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
	for(;cur;cur=fklPopArg(exe));
	fklResBp(exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_gt(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.>";
	int r=1;
	int err=0;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=fklPopArg(exe))
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
	for(;cur;cur=fklPopArg(exe));
	fklResBp(exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_ge(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.>=";
	int err=0;
	int r=1;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=fklPopArg(exe))
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
	for(;cur;cur=fklPopArg(exe));
	fklResBp(exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_lt(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.<";
	int err=0;
	int r=1;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=fklPopArg(exe))
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
	for(;cur;cur=fklPopArg(exe));
	fklResBp(exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_le(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.<=";
	int err=0;
	int r=1;
	DECL_AND_CHECK_ARG(cur,Pname);
	FklVMvalue* prev=NULL;
	for(;cur;cur=fklPopArg(exe))
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
	for(;cur;cur=fklPopArg(exe));
	fklResBp(exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static void builtin_char_to_integer(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.char->integer";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_CHR,Pname,exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(FKL_GET_CHR(obj)));
}

static void builtin_integer_to_char(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.integer->char";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsInt,Pname,exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_CHR(fklGetInt(obj)));
}

static void builtin_list_to_vector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.list->vector";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsList,Pname,exe);
	size_t len=fklVMlistLength(obj);
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	FklVMvec* vec=FKL_VM_VEC(r);
	for(size_t i=0;obj!=FKL_VM_NIL;i++,obj=FKL_VM_CDR(obj))
		fklSetRef(&vec->base[i],FKL_VM_CAR(obj),exe->gc);
	fklPushVMvalue(exe,r);
}

static void builtin_string_to_vector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string->vector";
	DECL_AND_CHECK_ARG(obj,Pname)
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FklString* str=FKL_VM_STR(obj);
	size_t len=str->size;
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	FklVMvec* vec=FKL_VM_VEC(r);
	for(size_t i=0;i<len;i++)
		fklSetRef(&vec->base[i],FKL_MAKE_VM_CHR(str->str[i]),exe->gc);
	fklPushVMvalue(exe,r);
}

static void builtin_make_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-list";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsInt,Pname,exe);
	FklVMvalue* content=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	FklVMvalue* t=content?content:FKL_VM_NIL;
	for(size_t i=0;i<len;i++)
	{
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,t),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_string_to_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string->list";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklString* str=FKL_VM_STR(obj);
	FklVMvalue** cur=&r;
	for(size_t i=0;i<str->size;i++)
	{
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_CHR(str->str[i])),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_bytevector_to_s8_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->s8-list";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	int8_t* s8a=(int8_t*)bvec->ptr;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_FIX(s8a[i])),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_bytevector_to_u8_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->s8-list";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	uint8_t* u8a=bvec->ptr;
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<size;i++)
	{
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_FIX(u8a[i])),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_bytevector_to_s8_vector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->s8-vector";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	int8_t* s8a=(int8_t*)bvec->ptr;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(size_t i=0;i<size;i++)
		fklSetRef(&v->base[i],FKL_MAKE_VM_FIX(s8a[i]),gc);
	fklPushVMvalue(exe,vec);
}

static void builtin_bytevector_to_u8_vector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->u8-vector";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	uint8_t* u8a=bvec->ptr;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(size_t i=0;i<size;i++)
		fklSetRef(&v->base[i],FKL_MAKE_VM_FIX(u8a[i]),gc);
	fklPushVMvalue(exe,vec);
}

static void builtin_vector_to_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vector->list";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* vec=FKL_VM_VEC(obj);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<vec->size;i++)
	{
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,vec->base[i]),gc);
		cur=&FKL_VM_CDR(*cur);
		fklDropTop(exe);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string";
	size_t size=exe->tp-exe->bp;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	size_t i=0;
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur!=NULL
			;cur=fklPopArg(exe),i++)
	{
		FKL_CHECK_TYPE(cur,FKL_IS_CHR,Pname,exe);
		str->str[i]=FKL_GET_CHR(cur);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_make_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-string";
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsInt,Pname,exe);
	FklVMvalue* content=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_make_vector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-vector";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsInt,Pname,exe);
	FklVMvalue* content=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	if(content)
	{
		FklVMvec* vec=FKL_VM_VEC(r);
		for(size_t i=0;i<len;i++)
			fklSetRef(&vec->base[i],content,gc);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_substring(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.substring";
	DECL_AND_CHECK_ARG3(ostr,vstart,vend,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_STR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsInt,Pname,exe);
	FKL_CHECK_TYPE(vend,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_sub_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.sub-string";
	DECL_AND_CHECK_ARG3(ostr,vstart,vsize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_STR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsInt,Pname,exe);
	FKL_CHECK_TYPE(vsize,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_subbytevector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.subbytevector";
	DECL_AND_CHECK_ARG3(ostr,vstart,vend,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsInt,Pname,exe);
	FKL_CHECK_TYPE(vend,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_sub_bytevector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.sub-bytevector";
	DECL_AND_CHECK_ARG3(ostr,vstart,vsize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsInt,Pname,exe);
	FKL_CHECK_TYPE(vsize,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_subvector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.subvector";
	DECL_AND_CHECK_ARG3(ovec,vstart,vend,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ovec,FKL_IS_VECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsInt,Pname,exe);
	FKL_CHECK_TYPE(vend,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_sub_vector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.sub-vector";
	DECL_AND_CHECK_ARG3(ovec,vstart,vsize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ovec,FKL_IS_VECTOR,Pname,exe);
	FKL_CHECK_TYPE(vstart,fklIsInt,Pname,exe);
	FKL_CHECK_TYPE(vsize,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
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
		if(fklIsInt(obj))
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
	fklPushVMvalue(exe,retval);
}

static void builtin_symbol_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.symbol->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_SYM,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe
			,fklCopyString(fklGetSymbolWithId(FKL_GET_SYM(obj),exe->symbolTable)->symbol));
	fklPushVMvalue(exe,retval);
}

static void builtin_string_to_symbol(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string->symbol";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_SYM(fklAddSymbol(FKL_VM_STR(obj),exe->symbolTable)->id));
}

static void builtin_symbol_to_integer(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.symbol->integer";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_SYM,Pname,exe);
	FklVMvalue* r=fklMakeVMint(FKL_GET_SYM(obj),exe);
	fklPushVMvalue(exe,r);
}

static void builtin_string_to_number(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string->number";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,Pname,exe);
	FklVMvalue* r=FKL_VM_NIL;
	FklString* str=FKL_VM_STR(obj);
	if(fklIsNumberString(str))
	{
		if(fklIsDoubleString(str))
			r=fklCreateVMvalueF64(exe,fklStringToDouble(str));
		else
		{
			FklBigInt bi=FKL_BIG_INT_INIT;
			fklInitBigIntFromString(&bi,str);
			if(!fklIsGtLtFixBigInt(&bi))
			{
				r=FKL_MAKE_VM_FIX(fklBigIntToI64(&bi));
				fklUninitBigInt(&bi);
			}
			else
			{
				r=fklCreateVMvalueBigInt(exe,NULL);
				*FKL_VM_BI(r)=bi;
			}
		}
	}
	fklPushVMvalue(exe,r);
}

static void builtin_number_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.number->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* radix=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	if(fklIsInt(obj))
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_CHECK_TYPE(radix,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,retval);
}

static void builtin_integer_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.integer->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* radix=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsInt,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	uint32_t base=10;
	if(radix)
	{
		FKL_CHECK_TYPE(radix,fklIsInt,Pname,exe);
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
	fklPushVMvalue(exe,retval);
}

static void builtin_f64_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.f64->string";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,FKL_IS_F64,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	char buf[64]={0};
	size_t size=snprintf(buf,64,"%lf",FKL_VM_F64(obj));
	FKL_VM_STR(retval)=fklCreateString(size,buf);
	fklPushVMvalue(exe,retval);
}

static void builtin_vector_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vector->string";
	DECL_AND_CHECK_ARG(vec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_bytevector_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.bytevector->string";
	DECL_AND_CHECK_ARG(vec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(vec,FKL_IS_BYTEVECTOR,Pname,exe);
	FklBytevector* bvec=FKL_VM_BVEC(vec);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(bvec->size,(char*)bvec->ptr));
	fklPushVMvalue(exe,r);
}

static void builtin_string_to_bytevector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string->bytevector";
	DECL_AND_CHECK_ARG(str,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(str,FKL_IS_STR,Pname,exe);
	FklString* s=FKL_VM_STR(str);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(s->size,(uint8_t*)s->str));
	fklPushVMvalue(exe,r);
}

static void builtin_vector_to_bytevector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vector->bytevector";
	DECL_AND_CHECK_ARG(vec,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(v->size,NULL));
	uint64_t size=v->size;
	FklVMvalue** base=v->base;
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(uint64_t i=0;i<size;i++)
	{
		FklVMvalue* cur=base[i];
		FKL_CHECK_TYPE(cur,fklIsInt,Pname,exe);
		ptr[i]=fklGetInt(cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_list_to_bytevector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.list->bytevector";
	DECL_AND_CHECK_ARG(list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(fklVMlistLength(list),NULL));
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(size_t i=0;list!=FKL_VM_NIL;i++,list=FKL_VM_CDR(list))
	{
		FklVMvalue* cur=FKL_VM_CAR(list);
		FKL_CHECK_TYPE(cur,fklIsInt,Pname,exe);
		ptr[i]=fklGetInt(cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_list_to_string(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.list->string";
	DECL_AND_CHECK_ARG(list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
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
	fklPushVMvalue(exe,r);
}

static void builtin_number_to_f64(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.integer->f64";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueF64(exe,0.0);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_FIX(obj))
		FKL_VM_F64(retval)=(double)FKL_GET_FIX(obj);
	else if(FKL_IS_BIG_INT(obj))
		FKL_VM_F64(retval)=fklBigIntToDouble(FKL_VM_BI(obj));
	else
		FKL_VM_F64(retval)=FKL_VM_F64(obj);
	fklPushVMvalue(exe,retval);
}

#include<math.h>

static void builtin_number_to_integer(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.number->integer";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,Pname,exe);
	if(FKL_IS_F64(obj))
	{
		double d=FKL_VM_F64(obj);
		if(isgreater(d,(double)FKL_FIX_INT_MAX)||isless(d,FKL_FIX_INT_MIN))
			fklPushVMvalue(exe,fklCreateVMvalueBigIntWithF64(exe,d));
		else
			fklPushVMvalue(exe,fklMakeVMintD(d,exe));
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
			fklPushVMvalue(exe,r);
		}
		else
			fklPushVMvalue(exe,fklMakeVMint(fklBigIntToI64(bigint),exe));
	}
	else
		fklPushVMvalue(exe,obj);
}

static void builtin_nth(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.nth";
	DECL_AND_CHECK_ARG2(place,objlist,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(place,fklIsInt,Pname,exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(uint64_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
			fklPushVMvalue(exe,FKL_VM_CAR(objPair));
		else
			fklPushVMvalue(exe,FKL_VM_NIL);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static void builtin_nth_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.nth-set!";
	DECL_AND_CHECK_ARG3(place,objlist,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(place,fklIsInt,Pname,exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(size_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			fklSetRef(&FKL_VM_CAR(objPair),target,exe->gc);
			fklPushVMvalue(exe,target);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDASSIGN,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static void builtin_sref(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.sref";
	DECL_AND_CHECK_ARG2(str,place,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklString* s=FKL_VM_STR(str);
	size_t size=s->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_CHR(s->str[index]));
}

#define BV_U_S_8_REF(TYPE,WHO) static const char Pname[]=(WHO);\
	DECL_AND_CHECK_ARG2(bvec,place,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	r=bv->ptr[index];\
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(r));\

#define BV_REF(TYPE,WHO,MAKER) static const char Pname[]=(WHO);\
	DECL_AND_CHECK_ARG2(bvec,place,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&r,&bv->ptr[index],sizeof(r));\
	fklPushVMvalue(exe,MAKER(r,exe));\

#define BV_S_REF(TYPE,WHO) BV_REF(TYPE,WHO,fklMakeVMint)
#define BV_U_REF(TYPE,WHO) BV_REF(TYPE,WHO,fklMakeVMuint)

static void builtin_bvs8ref(FKL_DL_PROC_ARGL) {BV_U_S_8_REF(int8_t,"builtin.bvs8ref")}
static void builtin_bvs16ref(FKL_DL_PROC_ARGL) {BV_S_REF(int16_t,"builtin.bvs16ref")}
static void builtin_bvs32ref(FKL_DL_PROC_ARGL) {BV_S_REF(int32_t,"builtin.bvs32ref")}
static void builtin_bvs64ref(FKL_DL_PROC_ARGL) {BV_S_REF(int64_t,"builtin.bvs64ref")}

static void builtin_bvu8ref(FKL_DL_PROC_ARGL) {BV_U_S_8_REF(uint8_t,"builtin.bvu8ref")}
static void builtin_bvu16ref(FKL_DL_PROC_ARGL) {BV_U_REF(uint16_t,"builtin.bvu16ref")}
static void builtin_bvu32ref(FKL_DL_PROC_ARGL) {BV_U_REF(uint32_t,"builtin.bvu32ref")}
static void builtin_bvu64ref(FKL_DL_PROC_ARGL) {BV_U_REF(uint64_t,"builtin.bvu64ref")}

#undef BV_REF
#undef BV_S_REF
#undef BV_U_REF
#undef BV_U_S_8_REF

#define BV_F_REF(TYPE,WHO) static const char Pname[]=(WHO);\
	DECL_AND_CHECK_ARG2(bvec,place,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=0;\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&r,&bv->ptr[index],sizeof(r));\
	fklPushVMvalue(exe,fklCreateVMvalueF64(exe,r));\

static void builtin_bvf32ref(FKL_DL_PROC_ARGL) {BV_F_REF(float,"builtin.bvf32ref")}
static void builtin_bvf64ref(FKL_DL_PROC_ARGL) {BV_F_REF(double,"builtin.bvf32ref")}
#undef BV_F_REF

#define SET_BV_S_U_8_REF(TYPE,WHO) static const char Pname[]=(WHO);\
	DECL_AND_CHECK_ARG3(bvec,place,target,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	bv->ptr[index]=r;\
	fklPushVMvalue(exe,target);\

#define SET_BV_REF(TYPE,WHO) static const char Pname[]=(WHO);\
	DECL_AND_CHECK_ARG3(bvec,place,target,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsInt(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=fklGetUint(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&bv->ptr[index],&r,sizeof(r));\
	fklPushVMvalue(exe,target);\

static void builtin_bvs8ref_set(FKL_DL_PROC_ARGL) {SET_BV_S_U_8_REF(int8_t,"builtin.bvs8ref-set!")}
static void builtin_bvs16ref_set(FKL_DL_PROC_ARGL) {SET_BV_REF(int16_t,"builtin.bvs16ref-set!")}
static void builtin_bvs32ref_set(FKL_DL_PROC_ARGL) {SET_BV_REF(int32_t,"builtin.bvs32ref-set!")}
static void builtin_bvs64ref_set(FKL_DL_PROC_ARGL) {SET_BV_REF(int64_t,"builtin.bvs64ref-set!")}

static void builtin_bvu8ref_set(FKL_DL_PROC_ARGL) {SET_BV_S_U_8_REF(uint8_t,"builtin.bvu8ref-set!")}
static void builtin_bvu16ref_set(FKL_DL_PROC_ARGL) {SET_BV_REF(uint16_t,"builtin.bvu16ref-set!")}
static void builtin_bvu32ref_set(FKL_DL_PROC_ARGL) {SET_BV_REF(uint32_t,"builtin.bvu32ref-set!")}
static void builtin_bvu64ref_set(FKL_DL_PROC_ARGL) {SET_BV_REF(uint64_t,"builtin.bvu64ref-set!")}

#undef SET_BV_S_U_8_REF
#undef SET_BV_REF

#define SET_BV_F_REF(TYPE,WHO) static const char Pname[]=(WHO);\
	DECL_AND_CHECK_ARG3(bvec,place,target,Pname);\
	FKL_CHECK_REST_ARG(exe,Pname,exe);\
	if(!fklIsInt(place)||!FKL_IS_BYTEVECTOR(bvec)||!FKL_IS_F64(target))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	TYPE r=FKL_VM_F64(target);\
	if(fklVMnumberLt0(place)||index>=size||size-index<sizeof(r))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);\
	memcpy(&bv->ptr[index],&r,sizeof(r));\
	fklPushVMvalue(exe,target);\

static void builtin_bvf32ref_set(FKL_DL_PROC_ARGL) {SET_BV_F_REF(float,"builtin.bvf32ref-set!")}
static void builtin_bvf64ref_set(FKL_DL_PROC_ARGL) {SET_BV_F_REF(double,"builtin.bvf64ref-set!")}
#undef SET_BV_F_REF

static void builtin_sref_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.sref-set!";
	DECL_AND_CHECK_ARG3(str,place,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklString* s=FKL_VM_STR(str);
	size_t size=s->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(!FKL_IS_CHR(target)&&!fklIsInt(target))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	s->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
	fklPushVMvalue(exe,target);
}

static void builtin_string_fill(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.string-fill!";
	DECL_AND_CHECK_ARG2(str,content,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!FKL_IS_CHR(content)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* s=FKL_VM_STR(str);
	memset(s->str,FKL_GET_CHR(content),s->size);
	fklPushVMvalue(exe,str);
}

static void builtin_bytevector_fill(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.bytevector-fill!";
	DECL_AND_CHECK_ARG2(bvec,content,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!fklIsInt(content)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklBytevector* bv=FKL_VM_BVEC(bvec);
	memset(bv->ptr,fklGetInt(content),bv->size);
	fklPushVMvalue(exe,bvec);
}

static void builtin_vref(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vref";
	DECL_AND_CHECK_ARG2(vec,place,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	fklPushVMvalue(exe,v->base[index]);
}

static void builtin_vref_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vref-set!";
	DECL_AND_CHECK_ARG3(vec,place,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	fklSetRef(&v->base[index],target,exe->gc);
	fklPushVMvalue(exe,target);
}

static void builtin_vector_fill(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vector-fill!";
	DECL_AND_CHECK_ARG2(vec,content,Pname)
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,Pname,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	for(size_t i=0;i<size;i++)
		fklSetRef(&v->base[i],content,exe->gc);
	fklPushVMvalue(exe,vec);
}

static void builtin_vref_cas(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.vref-cas!";
	FklVMvalue* vec=fklPopArg(exe);
	FklVMvalue* place=fklPopArg(exe);
	FklVMvalue* old=fklPopArg(exe);
	FklVMvalue* new=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!place||!vec||!old||!new)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(v->base[index]==old)
	{
		fklSetRef(&v->base[index],new,exe->gc);
		fklPushVMvalue(exe,FKL_VM_TRUE);
	}
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_nthcdr(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.nthcdr";
	DECL_AND_CHECK_ARG2(place,objlist,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(place,fklIsInt,Pname,exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(size_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
			fklPushVMvalue(exe,FKL_VM_CDR(objPair));
		else
			fklPushVMvalue(exe,FKL_VM_NIL);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static void builtin_tail(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.tail";
	DECL_AND_CHECK_ARG(objlist,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(;FKL_IS_PAIR(objPair)&&FKL_VM_CDR(objPair)!=FKL_VM_NIL;objPair=FKL_VM_CDR(objPair));
		fklPushVMvalue(exe,objPair);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static void builtin_nthcdr_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.nthcdr-set!";
	DECL_AND_CHECK_ARG3(place,objlist,target,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(place,fklIsInt,Pname,exe);
	size_t index=fklGetUint(place);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(size_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			fklSetRef(&FKL_VM_CDR(objPair),target,exe->gc);
			fklPushVMvalue(exe,target);
		}
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static void builtin_length(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.length";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
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
	fklPushVMvalue(exe,fklMakeVMuint(len,exe));
}

static void builtin_fopen(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fopen";
	DECL_AND_CHECK_ARG2(filename,mode,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* filenameStr=FKL_VM_STR(filename);
	FklString* modeStr=FKL_VM_STR(mode);
	FILE* file=fopen(filenameStr->str,modeStr->str);
	FklVMvalue* obj=NULL;
	if(!file)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,filenameStr->str,0,FKL_ERR_FILEFAILURE,exe);
	else
		obj=fklCreateVMvalueFp(exe,file,fklGetVMfpRwFromCstr(modeStr->str));
	fklPushVMvalue(exe,obj);
}

static void builtin_fclose(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fclose";
	DECL_AND_CHECK_ARG(vfp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(vfp,FKL_IS_FP,Pname,exe);
	FklVMfp* fp=FKL_VM_FP(vfp);
	if(fp->fp==NULL||fp->mutex||fklUninitVMfp(fp))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDACCESS,exe);
	fp->fp=NULL;
	fklPushVMvalue(exe,FKL_VM_NIL);
}

typedef struct
{
	FklVMvalue* fpv;
	FklStringMatchPattern* head;
	FklStringMatchRouteNode* root;
	FklStringMatchRouteNode* route;
	FklStringMatchSet* matchSet;
	FklStringBuffer* buf;
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
	fklDestroyStringBuffer(c->buf);
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

static void read_frame_step(FklCallObjData d,FklVM* exe)
{
	ReadCtx* rctx=(ReadCtx*)d;
	FklVMfp* vfp=FKL_VM_FP(rctx->fpv);
	FklStringBuffer* s=rctx->buf;
	int ch=fklVMfpNonBlockGetline(vfp,s);
	if(fklVMfpEof(vfp)||ch=='\n')
	{
		size_t line=1;
		int err=0;
		rctx->matchSet=fklSplitStringIntoTokenWithPattern(fklStringBufferBody(s)
				,fklStringBufferLen(s)
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
			size_t len=fklStringBufferLen(s);
			size_t errorLine=0;
			if(len-j)
				fklVMfpRewind(vfp,s,j);
			rctx->done=1;
			FklNastNode* node=fklCreateNastNodeFromTokenStackAndMatchRoute(rctx->tokenStack
					,rctx->root
					,&errorLine
					,rctx->headSymbol
					,NULL
					,exe->symbolTable);
			if(node==NULL)
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
			FklVMvalue* v=fklCreateVMvalueFromNastNode(exe,node,NULL);
			fklPushVMvalue(exe,v);
			fklDestroyNastNode(node);
		}
		else if(fklVMfpEof(vfp))
		{
			rctx->done=1;
			if(rctx->matchSet!=FKL_STRING_PATTERN_UNIVERSAL_SET)
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTEOF,exe);
			else
				fklPushVMvalue(exe,FKL_VM_NIL);
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
	ctx->buf=fklCreateStringBuffer();
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
	f->t=&ReadContextMethodTable;
	initReadCtx(f->data,fpv,patterns,ap,headSymbol);
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

#define CHECK_FP_READABLE(V,I,E) if(!isVMfpReadable(V))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(I,FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_WRITABLE(V,I,E) if(!isVMfpWritable(V))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(I,FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_OPEN(V,I,E) if(!FKL_VM_FP(V)->fp)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(I,FKL_ERR_INVALIDACCESS,E)

#define GET_OR_USE_STDOUT(VN) if(!VN)\
	VN=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(pd))->sysOut

#define GET_OR_USE_STDIN(VN) if(!VN)\
	VN=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(pd))->sysIn

static void builtin_read(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.read";
	FklVMvalue* stream=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(!stream||FKL_IS_FP(stream))
	{
		FKL_DECL_UD_DATA(pbd,PublicBuiltInData,FKL_VM_UD(pd));
		FklVMvalue* fpv=stream?stream:pbd->sysIn;
		CHECK_FP_READABLE(fpv,Pname,exe);
		CHECK_FP_OPEN(fpv,Pname,exe);
		initFrameToReadFrame(exe
				,fpv
				,pbd->patterns
				,0
				,pbd->builtInHeadSymbolTable);
	}
}

static void builtin_stringify(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.stringify";
	DECL_AND_CHECK_ARG(v,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,fklStringify(v,exe->symbolTable));
	fklPushVMvalue(exe,retval);
}

static void builtin_parse(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.parse";
	DECL_AND_CHECK_ARG(stream,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!FKL_IS_STR(stream))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	char* tmpString=NULL;
	FklPtrStack tokenStack=FKL_STACK_INIT;
	fklInitPtrStack(&tokenStack,32,16);
	FklStringMatchSet* matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	size_t line=1;
	size_t j=0;
	FKL_DECL_UD_DATA(pbd,PublicBuiltInData,FKL_VM_UD(pd));
	FklStringMatchPattern* patterns=pbd->patterns;
	FklStringMatchRouteNode* route=fklCreateStringMatchRouteNode(NULL,0,0,NULL,NULL,NULL);
	FklStringMatchRouteNode* troute=route;
	int err=0;
	FklString* ss=FKL_VM_STR(stream);
	fklSplitStringIntoTokenWithPattern(ss->str
			,ss->size
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
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDEXPR,exe);
	else
		tmp=fklCreateVMvalueFromNastNode(exe,node,NULL);
	while(!fklIsPtrStackEmpty(&tokenStack))
		fklDestroyToken(fklPopPtrStack(&tokenStack));
	fklUninitPtrStack(&tokenStack);
	fklPushVMvalue(exe,tmp);
	free(tmpString);
	fklDestroyNastNode(node);
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
	FklStringBuffer buf;
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
	fklInitStringBuffer(&ctx->buf);
}

static void fget_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
	FgetCtx* c=(FgetCtx*)data;
	fklGC_toGrey(c->fpv,gc);
}

static void fget_frame_finalizer(FklCallObjData data)
{
	FgetCtx* c=(FgetCtx*)data;
	fklUninitStringBuffer(&c->buf);
	fklUnLockVMfp(c->fpv);
}

static int fget_frame_end(FklCallObjData d)
{
	return ((FgetCtx*)d)->done;
}

static void fget_frame_step(FklCallObjData d,FklVM* exe)
{
	FgetCtx* ctx=(FgetCtx*)d;
	FklVMfp* vfp=FKL_VM_FP(ctx->fpv);
	int ch=0;
	FklStringBuffer* s=&ctx->buf;
	switch(ctx->mode)
	{
		case FGETC:
		case FGETI:
			ch=fklVMfpNonBlockGetc(vfp);
			if(fklVMfpEof(vfp))
			{
				ctx->done=1;
				fklPushVMvalue(exe,FKL_VM_NIL);
			}
			else if(ch>0)
			{
				ctx->done=1;
				FklVMvalue* retval=ctx->mode==FGETC?FKL_MAKE_VM_CHR(ch):FKL_MAKE_VM_FIX(ch);
				fklPushVMvalue(exe,retval);
			}
			break;
		case FGETD:
			fklVMfpNonBlockGetdelim(vfp,s,ctx->delim);
			if(fklVMfpEof(vfp)||ch==ctx->delim)
			{
				ctx->done=1;
				FklVMvalue* retval=fklCreateVMvalueStr(exe,fklStringBufferToString(s));
				fklPushVMvalue(exe,retval);
			}
			break;
		case FGETS:
		case FGETB:
			ctx->len-=fklVMfpNonBlockGets(vfp,s,ctx->len);
			if(!ctx->len||fklVMfpEof(vfp))
			{
				ctx->done=1;
				FklVMvalue* retval=ctx->mode==FGETS?fklCreateVMvalueStr(exe,fklStringBufferToString(s))
					:fklCreateVMvalueBvec(exe,fklStringBufferToBytevector(s));
				fklPushVMvalue(exe,retval);
			}
			break;
	}
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
	f->t=&FgetContextMethodTable;
	initFgetCtx(f->data,fpv,mode,len,del,ap);
}

static void builtin_fgetd(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fgetd";
	FklVMvalue* del=fklPopArg(exe);
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!del)
		del=FKL_MAKE_VM_CHR('\n');
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!FKL_IS_CHR(del))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_READABLE(file,Pname,exe);
	initFrameToFgetFrame(exe,file,FGETD,0,FKL_GET_CHR(del),0);
}

static void builtin_fgets(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fgets";
	DECL_AND_CHECK_ARG(psize,Pname);
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_READABLE(file,Pname,exe);
	initFrameToFgetFrame(exe,file,FGETS,fklGetUint(psize),0,0);
}

static void builtin_fgetb(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fgetb";
	DECL_AND_CHECK_ARG(psize,Pname);
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_READABLE(file,Pname,exe);
	initFrameToFgetFrame(exe,file,FGETB,fklGetUint(psize),0,0);
}

static void builtin_prin1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.prin1";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDOUT(file);
	if(!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);

	FILE* fp=FKL_VM_FP(file)->fp;
	fklPrin1VMvalue(obj,fp,exe->symbolTable);
	fklPushVMvalue(exe,obj);
}

static void builtin_princ(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.princ";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDOUT(file);
	if(!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);

	FILE* fp=FKL_VM_FP(file)->fp;
	fklPrincVMvalue(obj,fp,exe->symbolTable);
	fklPushVMvalue(exe,obj);
}

static void builtin_println(FKL_DL_PROC_ARGL)
{
	FklVMvalue* obj=fklPopArg(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=fklPopArg(exe))
		fklPrincVMvalue(obj,stdout,exe->symbolTable);
	fputc('\n',stdout);
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_print(FKL_DL_PROC_ARGL)
{
	FklVMvalue* obj=fklPopArg(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=fklPopArg(exe))
		fklPrincVMvalue(obj,stdout,exe->symbolTable);
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_fprint(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fprint";
	DECL_AND_CHECK_ARG(f,Pname);
	FKL_CHECK_TYPE(f,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(f,Pname,exe);
	CHECK_FP_WRITABLE(f,Pname,exe);
	FklVMvalue* obj=fklPopArg(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=fklPopArg(exe))
		fklPrincVMvalue(obj,fp,exe->symbolTable);
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_fprin1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fprin1";
	DECL_AND_CHECK_ARG(f,Pname);
	FKL_CHECK_TYPE(f,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(f,Pname,exe);
	CHECK_FP_WRITABLE(f,Pname,exe);

	FklVMvalue* obj=fklPopArg(exe);
	FklVMvalue* r=FKL_VM_NIL;
	FILE* fp=FKL_VM_FP(f)->fp;
	for(;obj;r=obj,obj=fklPopArg(exe))
		fklPrin1VMvalue(obj,fp,exe->symbolTable);
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_newline(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.newline";
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDOUT(file);
	FKL_CHECK_TYPE(file,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(file,Pname,exe);
	CHECK_FP_WRITABLE(file,Pname,exe);
	FILE* fp=FKL_VM_FP(file)->fp;
	fputc('\n',fp);
	fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_dlopen(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.dlopen";
	DECL_AND_CHECK_ARG(dllName,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(dllName,FKL_IS_STR,Pname,exe);
	FklString* dllNameStr=FKL_VM_STR(dllName);
	FklVMvalue* ndll=fklCreateVMvalueDll(exe,dllNameStr->str);
	if(!ndll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,dllNameStr->str,0,FKL_ERR_LOADDLLFAILD,exe);
	fklInitVMdll(ndll,exe);
	fklPushVMvalue(exe,ndll);
}

static void builtin_dlsym(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.dlsym";
	DECL_AND_CHECK_ARG2(ndll,symbol,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(ndll))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* ss=FKL_VM_STR(symbol);
	FklVMdll* dll=FKL_VM_DLL(ndll);
	FklVMdllFunc funcAddress=fklGetAddress(ss->str,dll->handle);
	if(!funcAddress)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,ss->str,0,FKL_ERR_INVALIDSYMBOL,exe);
	fklPushVMvalue(exe,fklCreateVMvalueDlproc(exe,funcAddress,ndll,dll->pd,0));
}

static void builtin_argv(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.argv";
	FklVMvalue* retval=NULL;
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&FKL_VM_CDR(*tmp))
		*tmp=fklCreateVMvaluePairWithCar(exe,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(argv[i])));
	fklPushVMvalue(exe,retval);
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
			fklVMhashTableSet(slotV,v1,ht,gc);
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

static void builtin_pmatch(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.pmatch";
	DECL_AND_CHECK_ARG2(pattern,exp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!isValidSyntaxPattern(pattern))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALIDPATTERN,exe);
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* hash=FKL_VM_HASH(r);
	if(matchPattern(pattern,exp,hash,exe->gc))
		fklPushVMvalue(exe,FKL_VM_NIL);
	else
		fklPushVMvalue(exe,r);
}

static void builtin_go(FKL_DL_PROC_ARGL)
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
	FklVMvalue* cur=fklPopArg(exe);
	FklPtrStack comStack=FKL_STACK_INIT;
	fklInitPtrStack(&comStack,32,16);
	for(;cur;cur=fklPopArg(exe))
		fklPushPtrStack(cur,&comStack);
	fklResBp(exe);
	while(!fklIsPtrStackEmpty(&comStack))
	{
		FklVMvalue* tmp=fklPopPtrStack(&comStack);
		fklPushVMvalue(threadVM,tmp);
	}
	fklUninitPtrStack(&comStack);
	FklVMvalue* chan=threadVM->chan;
	fklPushVMvalue(exe,chan);
}

static void builtin_chanl(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.chanl";
	DECL_AND_CHECK_ARG(maxSize,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(maxSize,fklIsInt,Pname,exe);
	if(fklVMnumberLt0(maxSize))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	fklPushVMvalue(exe,fklCreateVMvalueChanl(exe,fklGetUint(maxSize)));
}

static void builtin_chanl_msg_num(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.chanl#msg";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=FKL_VM_CHANL(obj)->messageNum;
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,fklMakeVMuint(len,exe));
}

static void builtin_chanl_send_num(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.chanl#send";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklLengthPtrQueue(&FKL_VM_CHANL(obj)->sendq);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,fklMakeVMuint(len,exe));
}

static void builtin_chanl_recv_num(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.chanl#recv";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklLengthPtrQueue(&FKL_VM_CHANL(obj)->recvq);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,fklMakeVMuint(len,exe));
}

static void builtin_chanl_full_p(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.chanl#recv";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FklVMvalue* retval=NULL;
	if(FKL_IS_CHAN(obj))
	{
		FklVMchanl* ch=FKL_VM_CHANL(obj);
		retval=ch->max>0&&ch->messageNum>=ch->max?FKL_MAKE_VM_FIX(1):FKL_VM_NIL;
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,retval);
}

static void builtin_chanl_msg_to_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.chanl-msg->list";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FklVMvalue* r=NULL;
	FklVMvalue** cur=&r;
	if(FKL_IS_CHAN(obj))
	{
		FklVMgc* gc=exe->gc;
		for(FklQueueNode* h=FKL_VM_CHANL(obj)->messages.head
				;h
				;h=h->next)
		{
			FklVMvalue* msg=h->data;
			fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,msg),gc);
			cur=&FKL_VM_CDR(*cur);
			fklDropTop(exe);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,r);
}



static void builtin_send(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.send";
	DECL_AND_CHECK_ARG2(ch,message,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,Pname,exe);
	fklChanlSend(message,FKL_VM_CHANL(ch),exe);
	fklPushVMvalue(exe,message);
}

static void builtin_recv(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.recv";
	DECL_AND_CHECK_ARG(ch,Pname);
	FklVMvalue* okBox=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,Pname,exe);
	FklVMchanl* chanl=FKL_VM_CHANL(ch);
	if(okBox)
	{
		FKL_CHECK_TYPE(okBox,FKL_IS_BOX,Pname,exe);
		FklVMvalue* r=FKL_VM_NIL;
		FKL_VM_BOX(okBox)=fklChanlRecvOk(chanl,&r)?FKL_VM_TRUE:FKL_VM_NIL;
		fklPushVMvalue(exe,r);
	}
	else
		fklChanlRecv(fklPushVMvalue(exe,FKL_VM_NIL),chanl,exe);
}

static void builtin_recv7(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.recv&";
	DECL_AND_CHECK_ARG(ch,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,Pname,exe);
	FklVMchanl* chanl=FKL_VM_CHANL(ch);
	FklVMvalue* r=FKL_VM_NIL;
	if(fklChanlRecvOk(chanl,&r))
		fklPushVMvalue(exe,fklCreateVMvalueBox(exe,r));
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_error(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.error";
	DECL_AND_CHECK_ARG3(type,who,message,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,fklCreateVMvalueError(exe
				,FKL_GET_SYM(type)
				,(FKL_IS_SYM(who))
				?fklGetSymbolWithId(FKL_GET_SYM(who),exe->symbolTable)->symbol
				:FKL_VM_STR(who)
				,fklCopyString(FKL_VM_STR(message))));
}

static void builtin_raise(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.raise";
	DECL_AND_CHECK_ARG(err,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,Pname,exe);
	fklPopVMframe(exe);
	fklRaiseVMerror(err,exe);
}

static void builtin_throw(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.throw";
	DECL_AND_CHECK_ARG3(type,who,message,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* err=fklCreateVMvalueError(exe
			,FKL_GET_SYM(type)
			,(FKL_IS_SYM(who))
			?fklGetSymbolWithId(FKL_GET_SYM(who),exe->symbolTable)->symbol
			:FKL_VM_STR(who)
			,fklCopyString(FKL_VM_STR(message)));
	fklPopVMframe(exe);
	fklRaiseVMerror(err,exe);
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

static void error_handler_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
	EhFrameContext* c=(EhFrameContext*)data;
	FklVMdlproc* dlproc=FKL_VM_DLPROC(c->proc);
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
			fklPushVMvalue(exe,errValue);
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
	for(FklVMvalue* v=fklPopArg(exe)
			;v
			;v=fklPopArg(exe))
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
		fklCallObj(proc,nf,exe);
	}
	else
	{
		fklUninitPtrStack(&errSymbolLists);
		fklUninitPtrStack(&errHandlers);
		fklTailCallObj(proc,exe->frames,exe);
	}
	fklSetBp(exe);
#undef GET_PROC
#undef GET_LIST
}

static void builtin_apply(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.apply";
	FklVMframe* frame=exe->frames;
	DECL_AND_CHECK_ARG(proc,Pname);
	FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
	FklPtrStack stack1=FKL_STACK_INIT;
	fklInitPtrStack(&stack1,32,16);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=fklPopArg(exe)))
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
		fklPushVMvalue(exe,t);
	}
	fklUninitPtrStack(&stack2);
	while(!fklIsPtrStackEmpty(&stack1))
	{
		FklVMvalue* t=fklPopPtrStack(&stack1);
		fklPushVMvalue(exe,t);
	}
	fklUninitPtrStack(&stack1);
	fklTailCallObj(proc,frame,exe);
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
}MapCtx;

#define K_MAP_PATTERN(K_FUNC,CUR_PROCESS,RESULT_PROCESS,NEXT_PROCESS) {MapCtx* mapctx=(MapCtx*)ctx;\
	FklVMgc* gc=exe->gc;\
	size_t len=mapctx->len;\
	size_t argNum=mapctx->num;\
	FklVMframe* frame=exe->frames;\
	if(s==FKL_CC_RE)\
	{\
		FklVMvalue* result=fklPopTopValue(exe);\
		RESULT_PROCESS\
		NEXT_PROCESS\
		mapctx->i++;\
	}\
	if(mapctx->i<len)\
	{\
		CUR_PROCESS\
		FklVMvec* carsv=FKL_VM_VEC(mapctx->cars);\
		FklVMvec* vec=FKL_VM_VEC(mapctx->vec);\
		for(size_t i=0;i<argNum;i++)\
		{\
			FklVMvalue* pair=vec->base[i];\
			fklSetRef(&carsv->base[i],FKL_VM_CAR(pair),gc);\
			fklSetRef(&vec->base[i],FKL_VM_CDR(pair),gc);\
		}\
		return fklCallInFuncK(mapctx->proc,argNum,carsv->base,frame,exe,(K_FUNC),mapctx,sizeof(MapCtx));\
	}\
	fklFuncKReturn(exe,FKL_GET_DLPROC_RTP(exe),*mapctx->r);}\

#define MAP_PATTERN(FUNC_NAME,K_FUNC,DEFAULT_VALUE) FklVMgc* gc=exe->gc;\
	DECL_AND_CHECK_ARG(proc,FUNC_NAME);\
	FKL_CHECK_TYPE(proc,fklIsCallable,(FUNC_NAME),exe);\
	size_t argNum=exe->tp-exe->bp;\
	if(argNum==0)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_TOOFEWARG,exe);\
	FklVMvalue* argVec=fklCreateVMvalueVec(exe,exe->tp-exe->bp);\
	FklVMvec* argv=FKL_VM_VEC(argVec);\
	for(size_t i=0;i<argNum;i++)\
	{\
		FklVMvalue* cur=fklPopArg(exe);\
		if(!fklIsList(cur))\
		FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
		fklSetRef(&argv->base[i],cur,gc);\
	}\
	fklResBp(exe);\
	size_t len=fklVMlistLength(argv->base[0]);\
	for(size_t i=1;i<argNum;i++)\
	if(fklVMlistLength(argv->base[i])!=len)\
	FKL_RAISE_BUILTIN_ERROR_CSTR((FUNC_NAME),FKL_ERR_LIST_DIFFER_IN_LENGTH,exe);\
	if(len==0)\
		fklPushVMvalue(exe,(DEFAULT_VALUE));\
	else\
	{\
		FklVMvalue* cars=fklCreateVMvalueVec(exe,argNum);\
		FklVMvalue* resultBox=fklCreateVMvalueBox(exe,(DEFAULT_VALUE));\
		MapCtx* mapctx=(MapCtx*)malloc(sizeof(MapCtx));\
		FKL_ASSERT(mapctx);\
		mapctx->proc=proc;\
		mapctx->r=&FKL_VM_BOX(resultBox);\
		mapctx->cars=cars;\
		mapctx->cur=mapctx->r;\
		mapctx->i=0;\
		mapctx->len=len;\
		mapctx->num=argNum;\
		mapctx->vec=argVec;\
		fklCallFuncK3((K_FUNC),exe,mapctx,exe->tp,resultBox,argVec,cars);}\

static void k_map(K_FUNC_ARGL) {K_MAP_PATTERN(k_map,
		*(mapctx->cur)=fklCreateVMvaluePairNil(exe);,
		fklSetRef(&FKL_VM_CAR(*mapctx->cur),result,gc);,
		mapctx->cur=&FKL_VM_CDR(*mapctx->cur);)}
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
	static const char Pname[]="builtin.memq";
	DECL_AND_CHECK_ARG2(obj,list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	FklVMvalue* r=list;
	for(;r!=FKL_VM_NIL;r=FKL_VM_CDR(r))
		if(FKL_VM_CAR(r)==obj)
			break;
	fklPushVMvalue(exe,r);
}

typedef struct
{
	FklVMvalue* obj;
	FklVMvalue* proc;
	FklVMvalue* list;
}MemberCtx;

static void k_member(K_FUNC_ARGL)
{
	MemberCtx* memberctx=(MemberCtx*)ctx;
	FklVMvalue* retval=FKL_VM_NIL;
	if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklPopTopValue(exe);
		if(result!=FKL_VM_NIL)
		{
			retval=memberctx->list;
			memberctx->list=FKL_VM_NIL;
		}
		else
			memberctx->list=FKL_VM_CDR(memberctx->list);
	}
	if(memberctx->list!=FKL_VM_NIL)
	{
		FklVMvalue* arglist[2]={memberctx->obj,FKL_VM_CAR(memberctx->list)};
		return fklCallInFuncK(memberctx->proc
				,2
				,arglist
				,exe->frames
				,exe
				,k_member
				,memberctx
				,sizeof(MemberCtx));
	}
	fklFuncKReturn(exe,FKL_GET_DLPROC_RTP(exe),retval);
}

static void builtin_member(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.member";
	DECL_AND_CHECK_ARG2(obj,list,Pname);
	FklVMvalue* proc=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	if(proc)
	{
		FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
		MemberCtx* memberctx=(MemberCtx*)malloc(sizeof(MemberCtx));
		FKL_ASSERT(memberctx);
		memberctx->obj=obj;
		memberctx->proc=proc;
		memberctx->list=list;
		fklCallFuncK(k_member,exe,memberctx,exe->tp);
		return;
	}
	FklVMvalue* r=list;
	for(;r!=FKL_VM_NIL;r=FKL_VM_CDR(r))
		if(fklVMvalueEqual(FKL_VM_CAR(r),obj))
			break;
	fklPushVMvalue(exe,r);
}

typedef struct
{
	FklVMvalue* proc;
	FklVMvalue* list;
}MempCtx;

static void k_memp(K_FUNC_ARGL)
{
	MempCtx* mempctx=(MempCtx*)ctx;
	FklVMvalue* retval=FKL_VM_NIL;
	if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklPopTopValue(exe);
		if(result!=FKL_VM_NIL)
		{
			retval=mempctx->list;
			mempctx->list=FKL_VM_NIL;
		}
		else
			mempctx->list=FKL_VM_CDR(mempctx->list);
	}
	if(mempctx->list!=FKL_VM_NIL)
	{
		return fklCallInFuncK(mempctx->proc
				,1,&FKL_VM_CAR(mempctx->list)
				,exe->frames,exe,k_memp,mempctx,sizeof(MempCtx));
	}
	fklFuncKReturn(exe,FKL_GET_DLPROC_RTP(exe),retval);
}

static void builtin_memp(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.memp";
	DECL_AND_CHECK_ARG2(proc,list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	MempCtx* mempctx=(MempCtx*)malloc(sizeof(MempCtx));
	FKL_ASSERT(mempctx);
	mempctx->proc=proc;
	mempctx->list=list;
	fklCallFuncK(k_memp,exe,mempctx,exe->tp);
}

typedef struct
{
	FklVMvalue** r;
	FklVMvalue** cur;
	FklVMvalue* proc;
	FklVMvalue* list;
}FilterCtx;

static void k_filter(K_FUNC_ARGL)
{
	FilterCtx* filterctx=(FilterCtx*)ctx;
	if(s==FKL_CC_RE)
	{
		FklVMvalue* result=fklPopTopValue(exe);
		if(result!=FKL_VM_NIL)
		{
			*filterctx->cur=fklCreateVMvaluePairNil(exe);
			fklSetRef(&FKL_VM_CAR(*filterctx->cur),FKL_VM_CAR(filterctx->list),exe->gc);
			filterctx->cur=&FKL_VM_CDR(*filterctx->cur);
		}
		filterctx->list=FKL_VM_CDR(filterctx->list);
	}
	if(filterctx->list!=FKL_VM_NIL)
	{
		return fklCallInFuncK(filterctx->proc
				,1,&FKL_VM_CAR(filterctx->list)
				,exe->frames,exe,k_filter,filterctx,sizeof(FilterCtx));
	}
	fklFuncKReturn(exe,FKL_GET_DLPROC_RTP(exe),*filterctx->r);
}

static void builtin_filter(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.filter";
	DECL_AND_CHECK_ARG2(proc,list,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(proc,fklIsCallable,Pname,exe);
	FKL_CHECK_TYPE(list,fklIsList,Pname,exe);
	FilterCtx* filterctx=(FilterCtx*)malloc(sizeof(FilterCtx));
	FKL_ASSERT(filterctx);
	FklVMvalue* resultBox=fklCreateVMvalueBoxNil(exe);
	filterctx->r=&FKL_VM_BOX(resultBox);
	filterctx->cur=filterctx->r;
	filterctx->proc=proc;
	filterctx->list=list;
	fklCallFuncK1(k_filter,exe,filterctx,exe->tp,resultBox);
}

static void builtin_list(FKL_DL_PROC_ARGL)
{
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pcur=&r;
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur
			;cur=fklPopArg(exe))
	{
		fklSetRef(pcur,fklCreateVMvaluePairWithCar(exe,cur),exe->gc);
		pcur=&FKL_VM_CDR(*pcur);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_list8(FKL_DL_PROC_ARGL)
{
	DECL_AND_CHECK_ARG(r,"builtin.list*");
	FklVMvalue** pcur=&r;
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur
			;cur=fklPopArg(exe))
	{
		*pcur=fklCreateVMvaluePair(exe,*pcur,cur);
		pcur=&FKL_VM_CDR(*pcur);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_reverse(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.reverse";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsList,Pname,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=FKL_VM_CDR(cdr))
		retval=fklCreateVMvaluePair(exe,FKL_VM_CAR(cdr),retval);
	fklPushVMvalue(exe,retval);
}

static void builtin_reverse1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.reverse!";
	DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(obj,fklIsList,Pname,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cdr=obj;
	FklVMgc* gc=exe->gc;
	while(cdr!=FKL_VM_NIL)
	{
		FklVMvalue* car=FKL_VM_CAR(cdr);
		FklVMvalue* pair=cdr;
		cdr=FKL_VM_CDR(cdr);
		fklSetRef(&FKL_VM_CAR(pair),car,gc);
		fklSetRef(&FKL_VM_CDR(pair),retval,gc);
		retval=pair;
	}
	fklPushVMvalue(exe,retval);
}

static void builtin_feof(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.feof";
	DECL_AND_CHECK_ARG(fp,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(fp,Pname,exe);
	fklPushVMvalue(exe,feof(FKL_VM_FP(fp)->fp)?FKL_VM_TRUE:FKL_VM_NIL);
}

static void builtin_vector(FKL_DL_PROC_ARGL)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(size_t i=0;i<size;i++)
		fklSetRef(&v->base[i],fklPopArg(exe),exe->gc);
	fklResBp(exe);
	fklPushVMvalue(exe,vec);
}

static void builtin_getcwd(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.getcwd";
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FklVMvalue* s=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(fklGetCwd()));
	fklPushVMvalue(exe,s);
}

static void builtin_cd(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.cd";
	DECL_AND_CHECK_ARG(dir,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(dir,FKL_IS_STR,Pname,exe);
	FklString* dirstr=FKL_VM_STR(dir);
	int r=fklChangeWorkPath(dirstr->str);
	if(r)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(Pname,dirstr->str,0,FKL_ERR_FILEFAILURE,exe);
	fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_fgetc(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fgetc";
	FklVMvalue* stream=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDIN(stream);
	FKL_CHECK_TYPE(stream,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(stream,Pname,exe);
	CHECK_FP_READABLE(stream,Pname,exe);
	initFrameToFgetFrame(exe,stream,FGETC,1,0,0);
}

static void builtin_fgeti(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fgeti";
	FklVMvalue* stream=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	GET_OR_USE_STDIN(stream);
	FKL_CHECK_TYPE(stream,FKL_IS_FP,Pname,exe);
	CHECK_FP_OPEN(stream,Pname,exe);
	CHECK_FP_READABLE(stream,Pname,exe);
	initFrameToFgetFrame(exe,stream,FGETI,1,0,0);
}

static void builtin_fwrite(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.fwrite";
	DECL_AND_CHECK_ARG(obj,Pname);
	FklVMvalue* file=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);

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
	fklPushVMvalue(exe,obj);
}

static void builtin_box(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.box";
	DECL_AND_SET_DEFAULT(obj,FKL_VM_NIL);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	fklPushVMvalue(exe,fklCreateVMvalueBox(exe,obj));
}

static void builtin_unbox(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.unbox";
	DECL_AND_CHECK_ARG(box,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,Pname,exe);
	fklPushVMvalue(exe,FKL_VM_BOX(box));
}

static void builtin_box_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.box-set!";
	DECL_AND_CHECK_ARG2(box,obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,Pname,exe);
	fklSetRef(&FKL_VM_BOX(box),obj,exe->gc);
	fklPushVMvalue(exe,obj);
}

static void builtin_box_cas(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.box-cas!";
	DECL_AND_CHECK_ARG3(box,old,new,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,Pname,exe);
	if(FKL_VM_BOX(box)==old)
	{
		fklSetRef(&FKL_VM_BOX(box),new,exe->gc);
		fklPushVMvalue(exe,FKL_VM_TRUE);
	}
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_bytevector(FKL_DL_PROC_ARGL)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(size,NULL));
	FklBytevector* bytevec=FKL_VM_BVEC(r);
	size_t i=0;
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur!=NULL
			;cur=fklPopArg(exe),i++)
	{
		FKL_CHECK_TYPE(cur,fklIsInt,"builtin.bytevector",exe);
		bytevec->ptr[i]=fklGetInt(cur);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);
}

static void builtin_make_bytevector(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-bytevector";
	DECL_AND_CHECK_ARG(size,Pname);
	FKL_CHECK_TYPE(size,fklIsInt,Pname,exe);
	FklVMvalue* content=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(len,NULL));
	FklBytevector* bytevec=FKL_VM_BVEC(r);
	uint8_t u_8=0;
	if(content)
	{
		FKL_CHECK_TYPE(content,fklIsInt,Pname,exe);
		u_8=fklGetInt(content);
	}
	memset(bytevec->ptr,u_8,len);
	fklPushVMvalue(exe,r);
}

#define PREDICATE(condtion,err_infor) DECL_AND_CHECK_ARG(val,err_infor);\
	FKL_CHECK_REST_ARG(exe,err_infor,exe);\
	if(condtion)\
	fklPushVMvalue(exe,FKL_VM_TRUE);\
	else\
	fklPushVMvalue(exe,FKL_VM_NIL);\

static void builtin_sleep(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.sleep";
	DECL_AND_CHECK_ARG(second,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(second,fklIsInt,Pname,exe);
	uint64_t sec=fklGetUint(second);
	fklSleepThread(exe,sec);
	fklPushVMvalue(exe,second);
}

static void builtin_srand(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.srand";
	FklVMvalue* s=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(s,fklIsInt,Pname,exe);
	srand(fklGetInt(s));
	fklPushVMvalue(exe,s);
}

static void builtin_rand(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.rand";
	FklVMvalue*  lim=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	if(lim&&!fklIsInt(lim))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(rand()%((lim==NULL)?RAND_MAX:fklGetInt(lim))));
}

#include<time.h>

static void builtin_get_time(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.get-time";
	FKL_CHECK_REST_ARG(exe,Pname,exe);
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
	char trueTime[timeLen];
	snprintf(trueTime,timeLen,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	FklVMvalue* tmpVMvalue=fklCreateVMvalueStr(exe,fklCreateString(timeLen-1,trueTime));
	fklPushVMvalue(exe,tmpVMvalue);
}

static void builtin_remove_file(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.remove-file";
	DECL_AND_CHECK_ARG(name,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	FklString* nameStr=FKL_VM_STR(name);
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(remove(nameStr->str)));
}

static void builtin_time(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.time";
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	fklPushVMvalue(exe,fklMakeVMint((int64_t)time(NULL),exe));
}

static void builtin_system(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.system";
	DECL_AND_CHECK_ARG(name,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(name,FKL_IS_STR,Pname,exe);
	FklString* nameStr=FKL_VM_STR(name);
	fklPushVMvalue(exe,fklMakeVMint(system(nameStr->str),exe));
}

static void builtin_hash(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hash";
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur
			;cur=fklPopArg(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht,exe->gc);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);;
}

static void builtin_hash_num(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hash-num";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	fklPushVMvalue(exe,fklMakeVMuint(FKL_VM_HASH(ht)->num,exe));
}

static void builtin_make_hash(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-hash";
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=fklPopArg(exe);key;key=fklPopArg(exe))
	{
		FklVMvalue* value=fklPopArg(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht,exe->gc);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);;
}

static void builtin_hasheqv(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hasheqv";
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur
			;cur=fklPopArg(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht,exe->gc);
	}
	fklPushVMvalue(exe,r);;
}

static void builtin_make_hasheqv(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-hasheqv";
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=fklPopArg(exe);key;key=fklPopArg(exe))
	{
		FklVMvalue* value=fklPopArg(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht,exe->gc);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);;
}

static void builtin_hashequal(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hashequal";
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=fklPopArg(exe)
			;cur
			;cur=fklPopArg(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht,exe->gc);
	}
	fklPushVMvalue(exe,r);;
}

static void builtin_make_hashequal(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.make-hashequal";
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=fklPopArg(exe);key;key=fklPopArg(exe))
	{
		FklVMvalue* value=fklPopArg(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht,exe->gc);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,r);;
}

static void builtin_href(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.href";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FklVMvalue* defa=fklPopArg(exe);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	int ok=0;
	FklVMvalue* retval=fklVMhashTableGet(key,FKL_VM_HASH(ht),&ok);
	if(ok)
		fklPushVMvalue(exe,retval);
	else
	{
		if(defa)
			fklPushVMvalue(exe,defa);
		else
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NO_VALUE_FOR_KEY,exe);
	}
}

static void builtin_hrefp(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hrefp";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMhashTableItem* i=fklVMhashTableGetItem(key,FKL_VM_HASH(ht));
	if(i)
		fklPushVMvalue(exe,fklCreateVMvaluePair(exe,i->key,i->v));
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_href7(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.href&";
	DECL_AND_CHECK_ARG2(ht,key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	int ok=0;
	FklVMvalue* retval=fklVMhashTableGet(key,FKL_VM_HASH(ht),&ok);
	if(ok)
		fklPushVMvalue(exe,fklCreateVMvalueBox(exe,retval));
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_href1(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.href!";
	DECL_AND_CHECK_ARG3(ht,key,toSet,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMhashTableItem* item=fklVMhashTableRef1(key,toSet,FKL_VM_HASH(ht),exe->gc);
	fklPushVMvalue(exe,item->v);
}

static void builtin_hash_clear(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hash-clear!";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	fklClearHashTable(FKL_VM_HASH(ht));
	fklPushVMvalue(exe,ht);
}

static void builtin_href_set(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.href-set!";
	DECL_AND_CHECK_ARG3(ht,key,value,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	fklVMhashTableSet(key,value,FKL_VM_HASH(ht),exe->gc);
	fklPushVMvalue(exe,value);;
}

static void builtin_href_set8(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.href=*!";
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklVMvalue* value=NULL;
	FklHashTable* hash=FKL_VM_HASH(ht);
	for(FklVMvalue* key=fklPopArg(exe);key;key=fklPopArg(exe))
	{
		value=fklPopArg(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,hash,exe->gc);
	}
	fklResBp(exe);
	fklPushVMvalue(exe,value);
}

static void builtin_hash_to_list(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hash->list";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklHashTable* hash=FKL_VM_HASH(ht);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		FklVMvalue* pair=fklCreateVMvaluePair(exe,item->key,item->v);
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,pair),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_hash_keys(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hash-keys";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklHashTable* hash=FKL_VM_HASH(ht);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,item->key),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
}

static void builtin_hash_values(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.hash-values";
	FklVMgc* gc=exe->gc;
	DECL_AND_CHECK_ARG(ht,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,Pname,exe);
	FklHashTable* hash=FKL_VM_HASH(ht);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		fklSetRef(cur,fklCreateVMvaluePairWithCar(exe,item->v),gc);
		cur=&FKL_VM_CDR(*cur);
	}
	fklPushVMvalue(exe,r);
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
	static const char Pname[]="builtin.odd?";
	DECL_AND_CHECK_ARG(val,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(val,fklIsInt,Pname,exe);
	int r=0;
	if(FKL_IS_FIX(val))
		r=FKL_GET_FIX(val)%2;
	else
		r=fklIsBigIntOdd(FKL_VM_BI(val));
	if(r)
		fklPushVMvalue(exe,FKL_VM_TRUE);
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static void builtin_even_p(FKL_DL_PROC_ARGL)
{
	static const char Pname[]="builtin.even?";
	DECL_AND_CHECK_ARG(val,Pname);
	FKL_CHECK_REST_ARG(exe,Pname,exe);
	FKL_CHECK_TYPE(val,fklIsInt,Pname,exe);
	int r=0;
	if(FKL_IS_FIX(val))
		r=FKL_GET_FIX(val)%2==0;
	else
		r=fklIsBigIntEven(FKL_VM_BI(val));
	if(r)
		fklPushVMvalue(exe,FKL_VM_TRUE);
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
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

static FklByteCodelnt* inlfunc_box0(INL_FUNC_ARGS)
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
	{"chanl#msg",             builtin_chanl_msg_num,           {NULL,         NULL,          NULL,          NULL,          }, },
	{"chanl#recv",            builtin_chanl_recv_num,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"chanl#send",            builtin_chanl_send_num,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"chanl-full?",           builtin_chanl_full_p,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"chanl-msg->list",       builtin_chanl_msg_to_list,       {NULL,         NULL,          NULL,          NULL,          }, },
	{"send",                  builtin_send,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"recv",                  builtin_recv,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"recv&",                 builtin_recv7,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"error",                 builtin_error,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"raise",                 builtin_raise,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"throw",                 builtin_throw,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"reverse",               builtin_reverse,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"reverse!",              builtin_reverse1,                {NULL,         NULL,          NULL,          NULL,          }, },
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
	{"sref-set!",             builtin_sref_set,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"string-fill!",          builtin_string_fill,             {NULL,         NULL,          NULL,          NULL,          }, },

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
	{"vref-set!",             builtin_vref_set,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"vref-cas!",             builtin_vref_cas,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"vector-fill!",          builtin_vector_fill,             {NULL,         NULL,          NULL,          NULL,          }, },

	{"list?",                 builtin_list_p,                  {NULL,         NULL,          NULL,          NULL,          }, },
	{"list",                  builtin_list,                    {NULL,         NULL,          NULL,          NULL,          }, },
	{"list*",                 builtin_list8,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"make-list",             builtin_make_list,               {NULL,         NULL,          NULL,          NULL,          }, },
	{"vector->list",          builtin_vector_to_list,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"string->list",          builtin_string_to_list,          {NULL,         NULL,          NULL,          NULL,          }, },
	{"nth-set!",              builtin_nth_set,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"nthcdr-set!",           builtin_nthcdr_set,              {NULL,         NULL,          NULL,          NULL,          }, },

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
	{"bvs8ref-set!",          builtin_bvs8ref_set,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvs16ref-set!",         builtin_bvs16ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvs32ref-set!",         builtin_bvs32ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvs64ref-set!",         builtin_bvs64ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu8ref-set!",          builtin_bvu8ref_set,             {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu16ref-set!",         builtin_bvu16ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu32ref-set!",         builtin_bvu32ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvu64ref-set!",         builtin_bvu64ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvf32ref-set!",         builtin_bvf32ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bvf64ref-set!",         builtin_bvf64ref_set,            {NULL,         NULL,          NULL,          NULL,          }, },
	{"bytevector-fill!",      builtin_bytevector_fill,         {NULL,         NULL,          NULL,          NULL,          }, },

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

	{"car-set!",              builtin_car_set,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"cdr-set!",              builtin_cdr_set,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"box",                   builtin_box,                     {inlfunc_box0, inlfunc_box,   NULL,          NULL,          }, },
	{"unbox",                 builtin_unbox,                   {NULL,         inlfunc_unbox, NULL,          NULL,          }, },
	{"box-set!",              builtin_box_set,                 {NULL,         NULL,          NULL,          NULL,          }, },
	{"box-cas!",              builtin_box_cas,                 {NULL,         NULL,          NULL,          NULL,          }, },
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
	{"href&",                 builtin_href7,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"hrefp",                 builtin_hrefp,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"href!",                 builtin_href1,                   {NULL,         NULL,          NULL,          NULL,          }, },
	{"href-set!",             builtin_href_set,                {NULL,         NULL,          NULL,          NULL,          }, },
	{"href-set*!",            builtin_href_set8,               {NULL,         NULL,          NULL,          NULL,          }, },
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

static inline void init_vm_public_data(PublicBuiltInData* pd,FklVM* exe)
{
	static const char* builtInHeadSymbolTableCstr[4]=
	{
		"quote",
		"qsquote",
		"unquote",
		"unqtesp",
	};
	FklSymbolTable* table=exe->symbolTable;
	FklVMvalue* builtInStdin=fklCreateVMvalueFp(exe,stdin,FKL_VM_FP_R);
	FklVMvalue* builtInStdout=fklCreateVMvalueFp(exe,stdout,FKL_VM_FP_W);
	FklVMvalue* builtInStderr=fklCreateVMvalueFp(exe,stderr,FKL_VM_FP_W);
	pd->sysIn=builtInStdin;
	pd->sysOut=builtInStdout;
	pd->sysErr=builtInStderr;
	pd->patterns=fklInitBuiltInStringPattern(table);
	for(int i=0;i<3;i++)
		pd->builtInHeadSymbolTable[i]=fklAddSymbolCstr(builtInHeadSymbolTableCstr[i],table)->id;
}

void fklInitGlobalVMclosure(FklVMframe* frame,FklVM* exe)
{
	FklVMCompoundFrameVarRef* f=&frame->c.lr;
	static const size_t RefCount=(sizeof(builtInSymbolList)/sizeof(struct SymbolFuncStruct))-1;
	f->rcount=RefCount;
	FklVMvarRef** closure=(FklVMvarRef**)malloc(sizeof(FklVMvarRef*)*f->rcount);
	FKL_ASSERT(closure);
	f->ref=closure;
	FklVMvalue* publicUserData=fklCreateVMvalueUdata(exe
			,0
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
		FklVMvalue* v=fklCreateVMvalueDlproc(exe
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
			,0
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
		FklVMvalue* v=fklCreateVMvalueDlproc(exe
				,builtInSymbolList[i].f
				,NULL
				,publicUserData
				,fklAddSymbolCstr(builtInSymbolList[i].s,table)->id);
		closure[i]=fklCreateClosedVMvarRef(v);
	}
}

