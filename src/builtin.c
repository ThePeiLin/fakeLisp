#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/grammer.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/base.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/builtin.h>
#include<fakeLisp/codegen.h>
#include<fakeLisp/bytecode.h>

#include<stdlib.h>
#include<string.h>
#include<math.h>
#ifdef _WIN32
#include<windows.h>
#else
#include<dlfcn.h>
#include<unistd.h>
#endif

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

static void _public_builtin_userdata_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(d,PublicBuiltInData,ud);
	fklVMgcToGray(d->sysIn,gc);
	fklVMgcToGray(d->sysOut,gc);
	fklVMgcToGray(d->sysErr,gc);
}

static FklVMudMetaTable PublicBuiltInDataMetaTable=
{
	.size=sizeof(PublicBuiltInData),
	.__atomic=_public_builtin_userdata_atomic,
};

//builtin functions

static int builtin_car(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(obj));
	return 0;
}

static int builtin_car_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,target,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,exe);
	FKL_VM_CAR(obj)=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_cdr(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_CDR(obj));
	return 0;
}

static int builtin_cdr_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,target,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,exe);
	FKL_VM_CDR(obj)=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_cons(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(car,cdr,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,car,cdr));
	return 0;
}

static int builtin_copy(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* retval=fklCopyVMvalue(obj,exe);
	if(!retval)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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

typedef FklVMvalue* (*VMvalueCopyAppender)(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top);

static FklVMvalue* __fkl_str_copy_append(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	uint64_t new_size=FKL_VM_STR(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_STR(cur))
			new_size+=FKL_VM_STR(cur)->size;
		else
			return NULL;
	}
	FklString* str=fklCreateString(new_size,NULL);
	new_size=FKL_VM_STR(v)->size;
	memcpy(str->str,FKL_VM_STR(v)->str,new_size*sizeof(char));
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss=FKL_VM_STR(cur)->size;
		memcpy(&str->str[new_size],FKL_VM_STR(cur)->str,ss*sizeof(char));
		new_size+=ss;
	}
	return fklCreateVMvalueStr(exe,str);
}

static FklVMvalue* __fkl_vec_copy_append(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	size_t new_size=FKL_VM_VEC(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_VECTOR(cur))
			new_size+=FKL_VM_VEC(cur)->size;
		else if(FKL_IS_DVECTOR(cur))
			new_size+=FKL_VM_DVEC(cur)->size;
		else
			return NULL;
	}
	FklVMvalue* new_vec_val=fklCreateVMvalueVec(exe,new_size);
	FklVMvec* new_vec=FKL_VM_VEC(new_vec_val);
	new_size=FKL_VM_VEC(v)->size;
	memcpy(new_vec->base,FKL_VM_VEC(v)->base,new_size*sizeof(FklVMvalue*));
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss;
		FklVMvalue** mem;
		if(FKL_IS_VECTOR(cur))
		{
			ss=FKL_VM_VEC(cur)->size;
			mem=FKL_VM_VEC(cur)->base;
		}
		else
		{
			ss=FKL_VM_DVEC(cur)->size;
			mem=FKL_VM_DVEC(cur)->base;
		}
		memcpy(&new_vec->base[new_size],mem,ss*sizeof(FklVMvalue*));
		new_size+=ss;
	}
	return new_vec_val;
}

static FklVMvalue* __fkl_pair_copy_append(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	if(argc)
	{
		FklVMvalue* retval=FKL_VM_NIL;
		FklVMvalue** prev=&retval;
		*prev=(FklVMvalue*)v;
		for(int64_t i=0;i<argc;i++)
		{
			FklVMvalue* pr=*prev;
			FklVMvalue* cur=top[-i];
			if(fklIsList(pr)
					&&(prev=copy_list(prev,exe),*prev==FKL_VM_NIL))
				*prev=cur;
			else
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		return retval;
	}
	else
		return (FklVMvalue*)v;
}

static FklVMvalue* __fkl_bytevector_copy_append(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	uint64_t new_size=FKL_VM_BVEC(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_BYTEVECTOR(cur))
			new_size+=FKL_VM_BVEC(cur)->size;
		else
			return NULL;
	}
	FklBytevector* bvec=fklCreateBytevector(new_size,NULL);
	new_size=FKL_VM_BVEC(v)->size;
	memcpy(bvec->ptr,FKL_VM_BVEC(v)->ptr,new_size*sizeof(char));
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss=FKL_VM_BVEC(cur)->size;
		memcpy(&bvec->ptr[new_size],FKL_VM_BVEC(cur)->ptr,ss*sizeof(char));
		new_size+=ss;
	}
	return fklCreateVMvalueBvec(exe,bvec);
}

static FklVMvalue* __fkl_userdata_copy_append(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	const FklVMud* ud=FKL_VM_UD(v);
	FklVMudCopyAppender appender=ud->t->__copy_append;
	if(appender)
		return appender(exe,ud,argc,top);
	else
		return NULL;
}

static FklVMvalue* __fkl_dvec_copy_append(FklVM* exe,const FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	size_t new_size=FKL_VM_DVEC(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_DVECTOR(cur))
			new_size+=FKL_VM_DVEC(cur)->size;
		else if(FKL_IS_VECTOR(cur))
			new_size+=FKL_VM_VEC(cur)->size;
		else
			return NULL;
	}
	FklVMvalue* new_vec_val=fklCreateVMvalueDvec(exe,new_size);
	FklVMdvec* new_vec=FKL_VM_DVEC(new_vec_val);
	new_size=FKL_VM_DVEC(v)->size;
	memcpy(new_vec->base,FKL_VM_DVEC(v)->base,new_size*sizeof(FklVMvalue*));
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss;
		FklVMvalue** mem;
		if(FKL_IS_DVECTOR(cur))
		{
			ss=FKL_VM_DVEC(cur)->size;
			mem=FKL_VM_DVEC(cur)->base;
		}
		else
		{
			ss=FKL_VM_VEC(cur)->size;
			mem=FKL_VM_VEC(cur)->base;
		}
		memcpy(&new_vec->base[new_size],mem,ss*sizeof(FklVMvalue*));
		new_size+=ss;
	}
	return new_vec_val;
}

static const VMvalueCopyAppender CopyAppenders[FKL_VM_VALUE_GC_TYPE_NUM]=
{
	NULL,
	NULL,
	__fkl_str_copy_append,
	__fkl_vec_copy_append,
	__fkl_pair_copy_append,
	NULL,
	__fkl_bytevector_copy_append,
	__fkl_userdata_copy_append,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	__fkl_dvec_copy_append,
	NULL,
	NULL,
};

static int builtin_append(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	if(!obj)
	{
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else if(FKL_IS_PTR(obj))
	{
		FklValueType type=obj->type;
		VMvalueCopyAppender copy_appender=CopyAppenders[type];
		if(copy_appender)
		{
			uint32_t argc=FKL_VM_GET_ARG_NUM(exe);
			if(argc)
			{
				FklVMvalue* r=copy_appender(exe,obj,argc,&FKL_VM_GET_TOP_VALUE(exe));
				if(!r)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				exe->tp-=argc;
				fklResBp(exe);
				FKL_VM_PUSH_VALUE(exe,r);
			}
			else
			{
				fklResBp(exe);
				FKL_VM_PUSH_VALUE(exe,obj);
			}
		}
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

typedef int (*VMvalueAppender)(FklVMvalue* v,uint32_t argc,FklVMvalue* const* top);

static int __fkl_str_append(FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	uint64_t new_size=FKL_VM_STR(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_STR(cur))
			new_size+=FKL_VM_STR(cur)->size;
		else
			return 1;
	}
	FklString* str=fklStringRealloc(FKL_VM_STR(v),new_size);
	new_size=str->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss=FKL_VM_STR(cur)->size;
		memcpy(&str->str[new_size],FKL_VM_STR(cur)->str,ss*sizeof(char));
		new_size+=ss;
	}
	str->size=new_size;
	FKL_VM_STR(v)=str;
	return 0;
}

static int __fkl_pair_append(FklVMvalue* obj,uint32_t argc,FklVMvalue* const* top)
{
	if(argc)
	{
		FklVMvalue* retval=FKL_VM_NIL;
		FklVMvalue** prev=&retval;
		*prev=obj;
		for(int64_t i=0;i<argc;i++)
		{
			FklVMvalue* cur=top[-i];
			for(FklVMvalue* head=get_initial_fast_value(*prev)
					;FKL_IS_PAIR(*prev)
					;prev=&FKL_VM_CDR(*prev)
					,head=get_fast_value(head))
				if(head==*prev)
					return 1;
			if(*prev==FKL_VM_NIL)
				*prev=cur;
			else
				return 1;
		}
	}
	return 0;
}

static int __fkl_bytevector_append(FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	uint64_t new_size=FKL_VM_BVEC(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_BYTEVECTOR(cur))
			new_size+=FKL_VM_BVEC(cur)->size;
		else
			return 1;
	}
	FklBytevector* bvec=fklBytevectorRealloc(FKL_VM_BVEC(v),new_size);
	new_size=bvec->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss=FKL_VM_BVEC(cur)->size;
		memcpy(&bvec->ptr[new_size],FKL_VM_BVEC(cur)->ptr,ss*sizeof(char));
		new_size+=ss;
	}
	bvec->size=new_size;
	FKL_VM_BVEC(v)=bvec;
	return 0;
}

static int __fkl_userdata_append(FklVMvalue* obj,uint32_t argc,FklVMvalue* const* top)
{
	FklVMud* ud=FKL_VM_UD(obj);
	FklVMudAppender appender=ud->t->__append;
	if(appender)
		return appender(ud,argc,top);
	else
		return 1;
}

static inline void dvec_reserve(FklVMdvec* v,size_t target)
{
	if(v->capacity>=target)
		return;
#ifdef WIN32
	size_t target_capacity=v->capacity+(v->capacity>>1);
#else
	size_t target_capacity=v->capacity<<1;
#endif
	if(target_capacity<target)
		target_capacity=target;
	FklVMvalue** base=(FklVMvalue**)fklRealloc(v->base,target_capacity*sizeof(FklVMvalue*));
	FKL_ASSERT(base);
	v->base=base;
	v->capacity=target_capacity;
}

static int __fkl_dvector_append(FklVMvalue* v,uint32_t argc,FklVMvalue* const* top)
{
	size_t new_size=FKL_VM_DVEC(v)->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		if(FKL_IS_DVECTOR(cur))
			new_size+=FKL_VM_DVEC(cur)->size;
		else if(FKL_IS_VECTOR(cur))
			new_size+=FKL_VM_VEC(cur)->size;
		else
			return 1;
	}
	FklVMdvec* dvec=FKL_VM_DVEC(v);
	dvec_reserve(dvec,new_size);
	new_size=dvec->size;
	for(int64_t i=0;i<argc;i++)
	{
		FklVMvalue* cur=top[-i];
		size_t ss;
		FklVMvalue** mem;
		if(FKL_IS_DVECTOR(cur))
		{
			ss=FKL_VM_DVEC(cur)->size;
			mem=FKL_VM_DVEC(cur)->base;
		}
		else
		{
			ss=FKL_VM_VEC(cur)->size;
			mem=FKL_VM_VEC(cur)->base;
		}
		memcpy(&dvec->base[new_size],mem,ss*sizeof(FklVMvalue*));
		new_size+=ss;
	}
	dvec->size=new_size;
	return 0;
}

static const VMvalueAppender Appenders[FKL_VM_VALUE_GC_TYPE_NUM]=
{
	NULL,
	NULL,
	__fkl_str_append,
	NULL,
	__fkl_pair_append,
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
	__fkl_dvector_append,
	NULL,
	NULL,
};

static int builtin_append1(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	if(!obj)
	{
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else if(FKL_IS_PTR(obj))
	{
		FklValueType type=obj->type;
		VMvalueAppender appender=Appenders[type];
		if(appender)
		{
			uint32_t argc=FKL_VM_GET_ARG_NUM(exe);
			if(argc)
			{
				if(appender(obj,argc,&FKL_VM_GET_TOP_VALUE(exe)))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				exe->tp-=argc;
				fklResBp(exe);
				FKL_VM_PUSH_VALUE(exe,obj);
			}
			else
			{
				fklResBp(exe);
				FKL_VM_PUSH_VALUE(exe,obj);
			}
		}
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_eq(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(fir,sec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe
			,fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_eqv(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(fir,sec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe
			,(fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_equal(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(fir,sec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe
			,(fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int builtin_add(FKL_CPROC_ARGL)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	for(;cur;cur=FKL_VM_POP_ARG(exe))
		if(fklProcessVMnumAdd(cur,&r64,&rd,&bi))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumAddResult(exe,r64,rd,&bi));
	return 0;
}

static int builtin_add_1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(arg,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* r=fklProcessVMnumInc(exe,arg);
	if(r)
		FKL_VM_PUSH_VALUE(exe,r);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_sub(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(prev,exe);
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	double rd=0.0;
	int64_t r64=0;
	FKL_CHECK_TYPE(prev,fklIsVMnumber,exe);
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
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,fklProcessVMnumSubResult(exe,prev,r64,rd,&bi));
	}
	return 0;
}

static int builtin_sub_1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(arg,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* r=fklProcessVMnumDec(exe,arg);
	if(r)
		FKL_VM_PUSH_VALUE(exe,r);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_mul(FKL_CPROC_ARGL)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	double rd=1.0;
	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=FKL_VM_POP_ARG(exe))
		if(fklProcessVMnumMul(cur,&r64,&rd,&bi))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumMulResult(exe,r64,rd,&bi));
	return 0;
}

static int builtin_idiv(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(prev,cur,exe);
	int64_t r64=1;
	FKL_CHECK_TYPE(prev,fklIsVMint,exe);
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	for(;cur;cur=FKL_VM_POP_ARG(exe))
		if(fklProcessVMintMul(cur,&r64,&bi))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(r64==0||FKL_IS_0_BIG_INT(&bi))
	{
		fklUninitBigInt(&bi);
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR,exe);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumIdivResult(exe,prev,r64,&bi));
	return 0;
}

static int builtin_div(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(prev,exe);
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	int64_t r64=1;
	double rd=1.0;
	FKL_CHECK_TYPE(prev,fklIsVMnumber,exe);
	if(!cur)
	{
		fklResBp(exe);
		FklVMvalue* r=fklProcessVMnumRec(exe,prev);
		if(!r)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR,exe);
		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		fklInitBigInt1(&bi);
		for(;cur;cur=FKL_VM_POP_ARG(exe))
			if(fklProcessVMnumMul(cur,&r64,&rd,&bi))
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		if(r64==0||FKL_IS_0_BIG_INT(&bi))
		{
			fklUninitBigInt(&bi);
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR,exe);
		}
		fklResBp(exe);
		FKL_VM_PUSH_VALUE(exe,fklProcessVMnumDivResult(exe,prev,r64,rd,&bi));
	}
	return 0;
}

static int builtin_mod(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(fir,sec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(fir,fklIsVMnumber,exe);
	FKL_CHECK_TYPE(sec,fklIsVMnumber,exe);
	FklVMvalue* r=fklProcessVMnumMod(exe,fir,sec);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR,exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_eqn(FKL_CPROC_ARGL)
{
	int r=1;
	int err=0;
	FKL_DECL_AND_CHECK_ARG(cur,exe);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)==0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
	int r=1;
	int err=0;
	FKL_DECL_AND_CHECK_ARG(cur,exe);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)>0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
	int err=0;
	int r=1;
	FKL_DECL_AND_CHECK_ARG(cur,exe);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)>=0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
	int err=0;
	int r=1;
	FKL_DECL_AND_CHECK_ARG(cur,exe);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)<0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
	int err=0;
	int r=1;
	FKL_DECL_AND_CHECK_ARG(cur,exe);
	FklVMvalue* prev=NULL;
	for(;cur;cur=FKL_VM_POP_ARG(exe))
	{
		if(prev)
		{
			r=fklVMvalueCmp(prev,cur,&err)<=0;
			if(err)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_CHR,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(FKL_GET_CHR(obj)));
	return 0;
}

static int builtin_integer_to_char(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsVMint,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_CHR(fklGetInt(obj)));
	return 0;
}

static int builtin_list_to_vector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsList,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe)
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,exe);
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
	FKL_DECL_AND_CHECK_ARG(size,exe);
	FKL_CHECK_TYPE(size,fklIsVMint,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(fklIsVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_VECTOR,exe);
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
	size_t size=exe->tp-exe->bp;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	size_t i=0;
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur!=NULL
			;cur=FKL_VM_POP_ARG(exe),i++)
	{
		FKL_CHECK_TYPE(cur,FKL_IS_CHR,exe);
		str->str[i]=FKL_GET_CHR(cur);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(size,exe);
	FKL_CHECK_TYPE(size,fklIsVMint,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(len,NULL));
	FklString* str=FKL_VM_STR(r);
	char ch=0;
	if(content)
	{
		FKL_CHECK_TYPE(content,FKL_IS_CHR,exe);
		ch=FKL_GET_CHR(content);
	}
	memset(str->str,ch,len);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_vector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(size,exe);
	FKL_CHECK_TYPE(size,fklIsVMint,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(fklIsVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	if(!content)
		content=FKL_VM_NIL;
	FklVMvec* vec=FKL_VM_VEC(r);
	for(size_t i=0;i<len;i++)
		vec->base[i]=content;
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_substring(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ostr,vstart,vend,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vend))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklString* str=FKL_VM_STR(ostr);
	size_t size=str->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,str->str+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ostr,vstart,vsize,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vsize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklString* str=FKL_VM_STR(ostr);
	size_t size=str->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(start+osize>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,str->str+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_subbytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ostr,vstart,vend,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vend))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklBytevector* bvec=FKL_VM_BVEC(ostr);
	size_t size=bvec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(size,bvec->ptr+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_bytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ostr,vstart,vsize,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ostr,FKL_IS_BYTEVECTOR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vsize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklBytevector* bvec=FKL_VM_BVEC(ostr);
	size_t size=bvec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(start+osize>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(size,bvec->ptr+start));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_subvector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ovec,vstart,vend,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ovec,FKL_IS_VECTOR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vend))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMvec* vec=FKL_VM_VEC(ovec);
	size_t size=vec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueVecWithPtr(exe,size,vec->base+start);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_vector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ovec,vstart,vsize,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ovec,FKL_IS_VECTOR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vsize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMvec* vec=FKL_VM_VEC(ovec);
	size_t size=vec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(start+osize>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueVecWithPtr(exe,size,vec->base+start);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

#include<fakeLisp/common.h>

static int builtin_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(FKL_IS_SYM(obj))
		retval=fklCreateVMvalueStr(exe,fklCopyString(fklVMgetSymbolWithId(exe->gc,FKL_GET_SYM(obj))->symbol));
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
				FKL_VM_STR(retval)=fklBigIntToString(FKL_VM_BI(obj),10,1,1);
			else
			{
				FklBigInt bi=FKL_BIG_INT_INIT;
				uint8_t t[64]={0};
				bi.size=64;
				bi.digits=t;
				fklSetBigIntI(&bi,fklGetInt(obj));
				FKL_VM_STR(retval)=fklBigIntToString(&bi,10,1,1);
			}
		}
		else
		{
			char buf[64]={0};
			size_t size=fklWriteDoubleToBuf(buf,64,FKL_VM_F64(obj));
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
			FKL_CHECK_TYPE(vec->base[i],FKL_IS_CHR,exe);
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
			FKL_CHECK_TYPE(FKL_VM_CAR(obj),FKL_IS_CHR,exe);
			str->str[i]=FKL_GET_CHR(FKL_VM_CAR(obj));
			obj=FKL_VM_CDR(obj);
		}
	}
	else if(FKL_IS_USERDATA(obj)&&fklIsAbleToStringUd(FKL_VM_UD(obj)))
	{
		FklStringBuffer buf;
		fklInitStringBuffer(&buf);
		fklUdToString(FKL_VM_UD(obj),&buf,exe->gc);
		FklString* str=fklStringBufferToString(&buf);
		fklUninitStringBuffer(&buf);
		retval=fklCreateVMvalueStr(exe,str);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_symbol_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_SYM,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe
			,fklCopyString(fklVMgetSymbolWithId(exe->gc,FKL_GET_SYM(obj))->symbol));
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_string_to_symbol(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(fklVMaddSymbol(exe->gc,FKL_VM_STR(obj))->id));
	return 0;
}

static int builtin_symbol_to_integer(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_SYM,exe);
	FklVMvalue* r=fklMakeVMint(FKL_GET_SYM(obj),exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_number(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FklVMvalue* radix=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	if(fklIsVMint(obj))
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_CHECK_TYPE(radix,fklIsVMint,exe);
			int64_t t=fklGetInt(radix);
			if(t!=8&&t!=10&&t!=16)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDRADIX_FOR_INTEGER,exe);
			base=t;
		}
		if(FKL_IS_BIG_INT(obj))
			FKL_VM_STR(retval)=fklBigIntToString(FKL_VM_BI(obj),base,1,1);
		else
		{
			FklBigInt bi=FKL_BIG_INT_INIT;
			uint8_t t[64]={0};
			bi.size=64;
			bi.digits=t;
			fklSetBigIntI(&bi,fklGetInt(obj));
			FKL_VM_STR(retval)=fklBigIntToString(&bi,base,1,1);
		}
	}
	else
	{
		uint32_t base=10;
		if(radix)
		{
			FKL_CHECK_TYPE(radix,fklIsVMint,exe);
			int64_t t=fklGetInt(radix);
			if(t!=10&&t!=16)
				FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDRADIX_FOR_FLOAT,exe);
			base=t;
		}
		char buf[64]={0};
		size_t size=0;
		if(base==10)
			size=fklWriteDoubleToBuf(buf,64,FKL_VM_F64(obj));
		else
			size=snprintf(buf,64,"%a",FKL_VM_F64(obj));
		FKL_VM_STR(retval)=fklCreateString(size,buf);
	}
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_integer_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FklVMvalue* radix=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsVMint,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	uint32_t base=10;
	if(radix)
	{
		FKL_CHECK_TYPE(radix,fklIsVMint,exe);
		int64_t t=fklGetInt(radix);
		if(t!=8&&t!=10&&t!=16)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDRADIX_FOR_INTEGER,exe);
		base=t;
	}
	if(FKL_IS_BIG_INT(obj))
		FKL_VM_STR(retval)=fklBigIntToString(FKL_VM_BI(obj),base,1,1);
	else
	{
		FklBigInt bi=FKL_BIG_INT_INIT;
		uint8_t t[64]={0};
		bi.size=64;
		bi.digits=t;
		fklSetBigIntI(&bi,fklGetInt(obj));
		FKL_VM_STR(retval)=fklBigIntToString(&bi,base,1,1);
	}
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_f64_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_F64,exe);
	FklVMvalue* retval=fklCreateVMvalueStr(exe,NULL);
	char buf[64]={0};
	size_t size=fklWriteDoubleToBuf(buf,64,FKL_VM_F64(obj));
	FKL_VM_STR(retval)=fklCreateString(size,buf);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_vector_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	for(size_t i=0;i<size;i++)
	{
		FKL_CHECK_TYPE(v->base[i],FKL_IS_CHR,exe);
		str->str[i]=FKL_GET_CHR(v->base[i]);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_bytevector_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_BYTEVECTOR,exe);
	FklBytevector* bvec=FKL_VM_BVEC(vec);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(bvec->size,(char*)bvec->ptr));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_bytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(str,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(str,FKL_IS_STR,exe);
	FklString* s=FKL_VM_STR(str);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(s->size,(uint8_t*)s->str));
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_vector_to_bytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(v->size,NULL));
	uint64_t size=v->size;
	FklVMvalue** base=v->base;
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(uint64_t i=0;i<size;i++)
	{
		FklVMvalue* cur=base[i];
		FKL_CHECK_TYPE(cur,fklIsVMint,exe);
		ptr[i]=fklGetInt(cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_list_to_bytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(list,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(list,fklIsList,exe);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(fklVMlistLength(list),NULL));
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(size_t i=0;list!=FKL_VM_NIL;i++,list=FKL_VM_CDR(list))
	{
		FklVMvalue* cur=FKL_VM_CAR(list);
		FKL_CHECK_TYPE(cur,fklIsVMint,exe);
		ptr[i]=fklGetInt(cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_list_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(list,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(list,fklIsList,exe);
	size_t size=fklVMlistLength(list);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	for(size_t i=0;i<size;i++)
	{
		FKL_CHECK_TYPE(FKL_VM_CAR(list),FKL_IS_CHR,exe);
		str->str[i]=FKL_GET_CHR(FKL_VM_CAR(list));
		list=FKL_VM_CDR(list);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_number_to_f64(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* retval=fklCreateVMvalueF64(exe,0.0);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsVMnumber,exe);
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
	FKL_DECL_AND_CHECK_ARG2(place,objlist,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(place,fklIsVMint,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_nth_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(place,objlist,target,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(place,fklIsVMint,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDASSIGN,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_str_ref(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(str,place,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	FklString* s=FKL_VM_STR(str);
	size_t size=s->size;
	if(index>=size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_CHR(s->str[index]));
	return 0;
}

#define BV_U_S_8_REF(TYPE) FKL_DECL_AND_CHECK_ARG2(bvec,place,exe);\
	FKL_CHECK_REST_ARG(exe);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	if(fklIsVMnumberLt0(place))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	if(index>=size||size-index<sizeof(TYPE))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);\
	TYPE r=bv->ptr[index];\
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(r));\
	return 0;

#define BV_REF(TYPE,MAKER) FKL_DECL_AND_CHECK_ARG2(bvec,place,exe);\
	FKL_CHECK_REST_ARG(exe);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	if(fklIsVMnumberLt0(place))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	if(index>=size||size-index<sizeof(TYPE))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);\
	TYPE r;\
	memcpy(&r,&bv->ptr[index],sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,MAKER(r,exe));\
	return 0;

#define BV_S_REF(TYPE) BV_REF(TYPE,fklMakeVMint)
#define BV_U_REF(TYPE) BV_REF(TYPE,fklMakeVMuint)

static int builtin_bvs8ref(FKL_CPROC_ARGL) {BV_U_S_8_REF(int8_t)}
static int builtin_bvs16ref(FKL_CPROC_ARGL) {BV_S_REF(int16_t)}
static int builtin_bvs32ref(FKL_CPROC_ARGL) {BV_S_REF(int32_t)}
static int builtin_bvs64ref(FKL_CPROC_ARGL) {BV_S_REF(int64_t)}

static int builtin_bvu8ref(FKL_CPROC_ARGL) {BV_U_S_8_REF(uint8_t)}
static int builtin_bvu16ref(FKL_CPROC_ARGL) {BV_U_REF(uint16_t)}
static int builtin_bvu32ref(FKL_CPROC_ARGL) {BV_U_REF(uint32_t)}
static int builtin_bvu64ref(FKL_CPROC_ARGL) {BV_U_REF(uint64_t)}

#undef BV_REF
#undef BV_S_REF
#undef BV_U_REF
#undef BV_U_S_8_REF

#define BV_F_REF(TYPE) FKL_DECL_AND_CHECK_ARG2(bvec,place,exe);\
	FKL_CHECK_REST_ARG(exe);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	if(fklIsVMnumberLt0(place))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	if(index>=size||size-index<sizeof(TYPE))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);\
	TYPE r;\
	memcpy(&r,&bv->ptr[index],sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,r));\
	return 0;

static int builtin_bvf32ref(FKL_CPROC_ARGL) {BV_F_REF(float)}
static int builtin_bvf64ref(FKL_CPROC_ARGL) {BV_F_REF(double)}
#undef BV_F_REF

#define SET_BV_S_U_8_REF(TYPE) FKL_DECL_AND_CHECK_ARG3(bvec,place,target,exe);\
	FKL_CHECK_REST_ARG(exe);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsVMint(target))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	if(fklIsVMnumberLt0(place))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	if(index>=size||size-index<sizeof(TYPE))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);\
	TYPE r=fklGetUint(target);\
	bv->ptr[index]=r;\
	FKL_VM_PUSH_VALUE(exe,target);\
	return 0;

#define SET_BV_REF(TYPE) FKL_DECL_AND_CHECK_ARG3(bvec,place,target,exe);\
	FKL_CHECK_REST_ARG(exe);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec)||!fklIsVMint(target))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	if(fklIsVMnumberLt0(place))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	if(index>=size||size-index<sizeof(TYPE))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);\
	TYPE r=fklGetUint(target);\
	memcpy(&bv->ptr[index],&r,sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,target);\
	return 0;

static int builtin_bvs8set1(FKL_CPROC_ARGL) {SET_BV_S_U_8_REF(int8_t)}
static int builtin_bvs16set1(FKL_CPROC_ARGL) {SET_BV_REF(int16_t)}
static int builtin_bvs32set1(FKL_CPROC_ARGL) {SET_BV_REF(int32_t)}
static int builtin_bvs64set1(FKL_CPROC_ARGL) {SET_BV_REF(int64_t)}

static int builtin_bvu8set1(FKL_CPROC_ARGL) {SET_BV_S_U_8_REF(uint8_t)}
static int builtin_bvu16set1(FKL_CPROC_ARGL) {SET_BV_REF(uint16_t)}
static int builtin_bvu32set1(FKL_CPROC_ARGL) {SET_BV_REF(uint32_t)}
static int builtin_bvu64set1(FKL_CPROC_ARGL) {SET_BV_REF(uint64_t)}

#undef SET_BV_S_U_8_REF
#undef SET_BV_REF

#define SET_BV_F_REF(TYPE) FKL_DECL_AND_CHECK_ARG3(bvec,place,target,exe);\
	FKL_CHECK_REST_ARG(exe);\
	if(!fklIsVMint(place)||!FKL_IS_BYTEVECTOR(bvec)||!FKL_IS_F64(target))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	if(fklIsVMnumberLt0(place))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);\
	size_t index=fklGetUint(place);\
	FklBytevector* bv=FKL_VM_BVEC(bvec);\
	size_t size=bv->size;\
	if(index>=size||size-index<sizeof(TYPE))\
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);\
	TYPE r=FKL_VM_F64(target);\
	memcpy(&bv->ptr[index],&r,sizeof(r));\
	FKL_VM_PUSH_VALUE(exe,target);\
	return 0;

static int builtin_bvf32set1(FKL_CPROC_ARGL) {SET_BV_F_REF(float)}
static int builtin_bvf64set1(FKL_CPROC_ARGL) {SET_BV_F_REF(double)}
#undef SET_BV_F_REF

static int builtin_str_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(str,place,target,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	FklString* s=FKL_VM_STR(str);
	size_t size=s->size;
	if(index>=size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	if(!FKL_IS_CHR(target)&&!fklIsVMint(target))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	s->str[index]=FKL_IS_CHR(target)?FKL_GET_CHR(target):fklGetInt(target);
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_string_fill(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(str,content,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!FKL_IS_CHR(content)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklString* s=FKL_VM_STR(str);
	memset(s->str,FKL_GET_CHR(content),s->size);
	FKL_VM_PUSH_VALUE(exe,str);
	return 0;
}

static int builtin_bytevector_fill(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(bvec,content,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(content)||!FKL_IS_BYTEVECTOR(bvec))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklBytevector* bv=FKL_VM_BVEC(bvec);
	memset(bv->ptr,fklGetInt(content),bv->size);
	FKL_VM_PUSH_VALUE(exe,bvec);
	return 0;
}

static int builtin_vec_ref(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,place,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(index>=size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[index]);
	return 0;
}

static int builtin_vec_first(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	if(!v->size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[0]);
	return 0;
}

static int builtin_vec_last(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	if(!v->size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[v->size-1]);
	return 0;
}

static int builtin_vec_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(vec,place,target,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	if(index>=size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	v->base[index]=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_vector_fill(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,content,exe)
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_VECTOR,exe);
	FklVMvec* v=FKL_VM_VEC(vec);
	size_t size=v->size;
	for(size_t i=0;i<size;i++)
		v->base[i]=content;
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_vec_cas(FKL_CPROC_ARGL)
{
	FklVMvalue* vec=FKL_VM_POP_ARG(exe);
	FklVMvalue* place=FKL_VM_POP_ARG(exe);
	FklVMvalue* old=FKL_VM_POP_ARG(exe);
	FklVMvalue* new=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(!place||!vec||!old||!new)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	size_t size=FKL_VM_VEC(vec)->size;
	if(index>=size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	if(FKL_VM_VEC_CAS(vec,index,&old,new))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_nthcdr(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(place,objlist,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(place,fklIsVMint,exe);
	size_t index=fklGetUint(place);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_tail(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(objlist,exe);
	FKL_CHECK_REST_ARG(exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(;FKL_IS_PAIR(objPair)&&FKL_VM_CDR(objPair)!=FKL_VM_NIL;objPair=FKL_VM_CDR(objPair));
		FKL_VM_PUSH_VALUE(exe,objPair);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_nthcdr_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(place,objlist,target,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(place,fklIsVMint,exe);
	size_t index=fklGetUint(place);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
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
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	return 0;
}

static int builtin_length(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	size_t len=0;
	if((obj==FKL_VM_NIL||FKL_IS_PAIR(obj))&&fklIsList(obj))
		len=fklVMlistLength(obj);
	else if(FKL_IS_STR(obj))
		len=FKL_VM_STR(obj)->size;
	else if(FKL_IS_VECTOR(obj))
		len=FKL_VM_VEC(obj)->size;
	else if(FKL_IS_DVECTOR(obj))
		len=FKL_VM_DVEC(obj)->size;
	else if(FKL_IS_BYTEVECTOR(obj))
		len=FKL_VM_BVEC(obj)->size;
	else if(FKL_IS_USERDATA(obj)&&fklUdHasLength(FKL_VM_UD(obj)))
		len=fklLengthVMud(FKL_VM_UD(obj));
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_fopen(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(filename,exe);
	FklVMvalue* mode=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(!FKL_IS_STR(filename)||(mode&&!FKL_IS_STR(mode)))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	const char* modeStr=mode?FKL_VM_STR(mode)->str:"r";
	FILE* fp=fopen(FKL_VM_STR(filename)->str,modeStr);
	FklVMvalue* obj=NULL;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE,exe,"Failed for file: %s",filename);
	obj=fklCreateVMvalueFp(exe,fp,fklGetVMfpRwFromCstr(modeStr));
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_fclose(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vfp,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vfp,FKL_IS_FP,exe);
	FklVMfp* fp=FKL_VM_FP(vfp);
	if(fp->fp==NULL||fklUninitVMfp(fp))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	fp->fp=NULL;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

typedef enum
{
	PARSE_CONTINUE=0,
	PARSE_REDUCING,
}ParsingState;

struct ParseCtx
{
	FklPtrStack symbolStack;
	FklUintStack lineStack;
	FklPtrStack stateStack;
	FklSid_t reducing_sid;
	uint32_t offset;
};

typedef struct
{
	FklSid_t sid;
	FklVMvalue* vfp;
	FklVMvalue* parser;
	FklStringBuffer buf;
	struct ParseCtx* pctx;
	ParsingState state;
}ReadCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReadCtx);

static void read_frame_atomic(void* data,FklVMgc* gc)
{
	ReadCtx* c=(ReadCtx*)data;
	fklVMgcToGray(c->vfp,gc);
	fklVMgcToGray(c->parser,gc);
	struct ParseCtx* pctx=c->pctx;
	FklAnalysisSymbol** base=(FklAnalysisSymbol**)pctx->symbolStack.base;
	size_t len=pctx->symbolStack.top;
	for(size_t i=0;i<len;i++)
		fklVMgcToGray(base[i]->ast,gc);
}

static void read_frame_finalizer(void* data)
{
	ReadCtx* c=(ReadCtx*)data;
	struct ParseCtx* pctx=c->pctx;
	fklUninitStringBuffer(&c->buf);
	FklPtrStack* ss=&pctx->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
		free(fklPopPtrStack(ss));
	fklUninitPtrStack(ss);
	fklUninitUintStack(&pctx->lineStack);
	fklUninitPtrStack(&pctx->stateStack);
	free(pctx);
}

static int read_frame_step(void* d,FklVM* exe)
{
	ReadCtx* rctx=(ReadCtx*)d;
	FklVMfp* vfp=FKL_VM_FP(rctx->vfp);
	struct ParseCtx* pctx=rctx->pctx;
	FklStringBuffer* s=&rctx->buf;
	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);

	int err=0;
	size_t restLen=fklStringBufferLen(s)-pctx->offset;
	size_t errLine=0;

	FklVMvalue* ast=fklDefaultParseForCharBuf(fklStringBufferBody(s)+pctx->offset
			,restLen
			,&restLen
			,&outerCtx
			,&err
			,&errLine
			,&pctx->symbolStack
			,&pctx->lineStack
			,&pctx->stateStack);

	pctx->offset=fklStringBufferLen(s)-restLen;

	if(pctx->symbolStack.top==0&&fklVMfpEof(vfp))
	{
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueEof(exe));
		return 0;
	}
	else if((err==FKL_PARSE_WAITING_FOR_MORE
				||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			&&fklVMfpEof(vfp))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF,exe);
	else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR,exe);
	else if(err==FKL_PARSE_REDUCE_FAILED)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR,exe);
	else if(ast)
	{
		if(restLen)
			fklVMfpRewind(vfp,s,fklStringBufferLen(s)-restLen);
		FKL_VM_PUSH_VALUE(exe,ast);
		return 0;
	}
	else
		fklVMread(exe,FKL_VM_FP(rctx->vfp)->fp,&rctx->buf,1,'\n');
	return 1;
}

static void read_frame_print_backtrace(void* d,FILE* fp,FklVMgc* gc)
{
	fklDoPrintCprocBacktrace(((ReadCtx*)d)->sid,fp,gc);
}

static const FklVMframeContextMethodTable ReadContextMethodTable=
{
	.atomic=read_frame_atomic,
	.finalizer=read_frame_finalizer,
	.copy=NULL,
	.print_backtrace=read_frame_print_backtrace,
	.step=read_frame_step,
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
	FKL_DECL_VM_UD_DATA(g,FklGrammer,parser);
	fklPushPtrStack(&g->aTable.states[0],stack);
}

static inline struct ParseCtx* create_read_parse_ctx(void)
{
	struct ParseCtx* pctx=(struct ParseCtx*)malloc(sizeof(struct ParseCtx));
	FKL_ASSERT(pctx);
	pctx->offset=0;
	pctx->reducing_sid=0;
	fklInitPtrStack(&pctx->stateStack,16,16);
	fklInitPtrStack(&pctx->symbolStack,16,16);
	fklInitUintStack(&pctx->lineStack,16,16);
	return pctx;
}

static inline void initReadCtx(void* data
		,FklSid_t sid
		,FklVM* exe
		,FklVMvalue* vfp
		,FklVMvalue* parser)
{
	ReadCtx* ctx=(ReadCtx*)data;
	ctx->sid=sid;
	ctx->parser=parser;
	ctx->vfp=vfp;
	fklInitStringBuffer(&ctx->buf);
	struct ParseCtx* pctx=create_read_parse_ctx();
	ctx->pctx=pctx;
	if(parser==FKL_VM_NIL)
		fklVMvaluePushState0ToStack(&pctx->stateStack);
	else
		push_state0_of_custom_parser(parser,&pctx->stateStack);
	ctx->state=PARSE_CONTINUE;
	fklVMread(exe
			,FKL_VM_FP(vfp)->fp
			,&ctx->buf
			,1
			,'\n');
}

static inline int do_custom_parser_reduce_action(FklPtrStack* stateStack
		,FklPtrStack* symbolStack
		,FklUintStack* lineStack
		,const FklGrammerProduction* prod
		,FklGrammerMatchOuterCtx* outerCtx
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
		nodes=(void**)malloc(len*sizeof(void*));
		FKL_ASSERT(nodes||!len);
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
	prod->func(prod->ctx,outerCtx->ctx,nodes,len,line);
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

static int custom_read_frame_step(void* d,FklVM* exe)
{
	ReadCtx* rctx=(ReadCtx*)d;
	FklVMfp* vfp=FKL_VM_FP(rctx->vfp);
	struct ParseCtx* pctx=rctx->pctx;
	FklStringBuffer* s=&rctx->buf;

	if(rctx->state==PARSE_REDUCING)
	{
		FklVMvalue* ast=fklPopTopValue(exe);
		fklPushPtrStack(create_nonterm_analyzing_symbol(pctx->reducing_sid,ast),&pctx->symbolStack);
		rctx->state=PARSE_CONTINUE;
	}

	FKL_DECL_VM_UD_DATA(g,FklGrammer,rctx->parser);

	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);

	int err=0;
	int accept=0;
	size_t restLen=fklStringBufferLen(s)-pctx->offset;
	size_t errLine=0;

	parse_with_custom_parser_for_char_buf(g
			,fklStringBufferBody(s)+pctx->offset
			,restLen
			,&restLen
			,&outerCtx
			,&err
			,&errLine
			,&pctx->symbolStack
			,&pctx->lineStack
			,&pctx->stateStack
			,exe
			,&accept
			,&rctx->state
			,&pctx->reducing_sid);

	if(accept)
	{
		if(restLen)
			fklVMfpRewind(vfp,s,fklStringBufferLen(s)-restLen);
		FklAnalysisSymbol* top=fklPopPtrStack(&pctx->symbolStack);
		FKL_VM_PUSH_VALUE(exe,top->ast);
		free(top);
		return 0;
	}
	else if(pctx->symbolStack.top==0&&fklVMfpEof(vfp))
	{
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return 0;
	}
	else if((err==FKL_PARSE_WAITING_FOR_MORE
				||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			&&fklVMfpEof(vfp))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF,exe);
	else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR,exe);
	else if(err==FKL_PARSE_REDUCE_FAILED)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR,exe);
	else
	{
		pctx->offset=fklStringBufferLen(s)-restLen;
		if(rctx->state==PARSE_CONTINUE)
			fklVMread(exe,FKL_VM_FP(rctx->vfp)->fp,&rctx->buf,1,'\n');
	}
	return 1;
}

static const FklVMframeContextMethodTable CustomReadContextMethodTable=
{
	.atomic=read_frame_atomic,
	.finalizer=read_frame_finalizer,
	.copy=NULL,
	.print_backtrace=read_frame_print_backtrace,
	.step=custom_read_frame_step,
};

static inline void init_custom_read_frame(FklVM* exe
		,FklSid_t sid
		,FklVMvalue* parser
		,FklVMvalue* vfp)
{
	FklVMframe* f=exe->top_frame;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&CustomReadContextMethodTable;
	initReadCtx(f->data,sid,exe,vfp,parser);
}

static inline void initFrameToReadFrame(FklVM* exe
		,FklSid_t sid
		,FklVMvalue* vfp)
{
	FklVMframe* f=exe->top_frame;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&ReadContextMethodTable;
	initReadCtx(f->data,sid,exe,vfp,FKL_VM_NIL);
}


static inline int isVMfpReadable(const FklVMvalue* fp)
{
	return FKL_VM_FP(fp)->rw&FKL_VM_FP_R_MASK;
}

static inline int isVMfpWritable(const FklVMvalue* fp)
{
	return FKL_VM_FP(fp)->rw&FKL_VM_FP_W_MASK;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(custom_parser_as_print,parser);

static void custom_parser_finalizer(FklVMud* p)
{
	FKL_DECL_UD_DATA(grammer,FklGrammer,p);
	fklUninitGrammer(grammer);
}

static void custom_parser_atomic(const FklVMud* p,FklVMgc* gc)
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
		,size_t line)
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
	FklSid_t sid;
	const char* func_name;
	FklVMvalue* box;
	FklVMvalue* parser;
	FklVMvalue* str;
	struct ParseCtx* pctx;
	ParsingState state;
}CustomParseCtx;

static const FklVMudMetaTable CustomParserMetaTable=
{
	.size=sizeof(FklGrammer),
	.__as_princ=custom_parser_as_print,
	.__as_prin1=custom_parser_as_print,
	.__atomic=custom_parser_atomic,
	.__finalizer=custom_parser_finalizer,
};

#define REGEX_COMPILE_ERROR (1)
#define INVALID_PROD_PART (2)

static inline FklGrammerProduction* vm_vec_to_prod(FklVMvec* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklSid_t left
		,int* error_type)
{

	FklPtrStack valid_items;
	fklInitPtrStack(&valid_items,vec->size,8);

	uint8_t* delim=(uint8_t*)malloc(vec->size*sizeof(uint8_t));
	FKL_ASSERT(delim||!vec->size);
	memset(delim,1,sizeof(uint8_t)*vec->size);

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
		else if(FKL_IS_VECTOR(cur)
				&&FKL_VM_VEC(cur)->size==1
				&&FKL_IS_STR(FKL_VM_VEC(cur)->base[0]))
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur==FKL_VM_NIL)
		{
			delim[valid_items.top]=0;
			continue;
		}
		else
		{
			*error_type=INVALID_PROD_PART;
			goto end;
		}
	}
	FklGrammerSym* syms=NULL;
	if(valid_items.top)
	{
		size_t top=valid_items.top;
		syms=(FklGrammerSym*)malloc(valid_items.top*sizeof(FklGrammerSym));
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
				ss->end_with_terminal=0;
			}
			else if(FKL_IS_VECTOR(cur))
			{
				ss->isterm=1;
				ss->term_type=FKL_TERM_STRING;
				ss->nt.group=0;
				ss->nt.sid=fklAddSymbol(FKL_VM_STR(FKL_VM_VEC(cur)->base[0]),tt)->id;
				ss->end_with_terminal=1;
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
					*error_type=REGEX_COMPILE_ERROR;
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
	return prod;
}

static int builtin_make_parser(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(start,exe);
	FKL_CHECK_TYPE(start,FKL_IS_SYM,exe);
	FklVMvalue* retval=fklCreateVMvalueUd(exe,&CustomParserMetaTable,FKL_VM_NIL);
	FKL_DECL_VM_UD_DATA(grammer,FklGrammer,retval);
	fklVMacquireSt(exe->gc);
	fklInitEmptyGrammer(grammer,exe->gc->st);
	fklVMreleaseSt(exe->gc);
	grammer->start=(FklGrammerNonterm){.group=0,.sid=FKL_GET_SYM(start)};

	if(fklAddExtraProdToGrammer(grammer))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_GRAMMER_CREATE_FAILED,exe);

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
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				break;
			case EXCEPT_NEXT_ARG_VECTOR:
				if(FKL_IS_VECTOR(next_arg))
				{
					int error_type=0;
					prod=vm_vec_to_prod(FKL_VM_VEC(next_arg),builtins,tt,rt,sid,&error_type);
					if(!prod)
					{
						if(error_type==REGEX_COMPILE_ERROR)
							FKL_RAISE_BUILTIN_ERROR(FKL_ERR_REGEX_COMPILE_FAILED,exe);
						else
							FKL_RAISE_BUILTIN_ERROR(FKL_ERR_GRAMMER_CREATE_FAILED,exe);
					}
					next=EXCEPT_NEXT_ARG_CALLABLE;
					if(is_adding_ignore)
					{
						FklGrammerIgnore* ig=fklGrammerSymbolsToIgnore(prod->syms,prod->len,tt);
						fklDestroyGrammerProduction(prod);
						if(!ig)
							FKL_RAISE_BUILTIN_ERROR(FKL_ERR_GRAMMER_CREATE_FAILED,exe);
						next_arg=EXCEPT_NEXT_ARG_SYMBOL;
						if(fklAddIgnoreToIgnoreList(&grammer->ignores,ig))
						{
							fklDestroyIgnore(ig);
							FKL_RAISE_BUILTIN_ERROR(FKL_ERR_GRAMMER_CREATE_FAILED,exe);
						}
						next=EXCEPT_NEXT_ARG_SYMBOL;
					}
				}
				else
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				break;
			case EXCEPT_NEXT_ARG_CALLABLE:
				next=EXCEPT_NEXT_ARG_SYMBOL;
				if(fklIsCallable(next_arg))
				{
					prod->ctx=next_arg;
					if(fklAddProdAndExtraToGrammer(grammer,prod))
					{
						fklDestroyGrammerProduction(prod);
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_GRAMMER_CREATE_FAILED,exe);
					}
				}
				else
				{
					fklDestroyGrammerProduction(prod);
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				}
				break;
		}
	}
	if(next!=EXCEPT_NEXT_ARG_SYMBOL)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	fklResBp(exe);

#undef EXCEPT_NEXT_ARG_SYMBOL
#undef EXCEPT_NEXT_ARG_VECTOR
#undef EXCEPT_NEXT_ARG_CALLABLE

	if(fklCheckAndInitGrammerSymbols(grammer))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_GRAMMER_CREATE_FAILED,exe);

	FklHashTable* itemSet=fklGenerateLr0Items(grammer);
	fklLr0ToLalrItems(itemSet,grammer);

	if(fklGenerateLalrAnalyzeTable(grammer,itemSet))
	{
		fklDestroyHashTable(itemSet);
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_ANALYSIS_TABLE_GENERATE_FAILED,exe);
	}
	fklDestroyHashTable(itemSet);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

#undef REGEX_COMPILE_ERROR
#undef INVALID_PROD_PART

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
	FklAnalysisSymbol** base=(FklAnalysisSymbol**)c->pctx->symbolStack.base;
	size_t len=c->pctx->symbolStack.top;
	for(size_t i=0;i<len;i++)
		fklVMgcToGray(base[i]->ast,gc);
}

static void custom_parse_frame_finalizer(void* data)
{
	CustomParseCtx* c=(CustomParseCtx*)data;
	FklPtrStack* ss=&c->pctx->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
		free(fklPopPtrStack(ss));
	fklUninitPtrStack(ss);
	fklUninitUintStack(&c->pctx->lineStack);
	fklUninitPtrStack(&c->pctx->stateStack);
	free(c->pctx);
}

static int custom_parse_frame_step(void* d,FklVM* exe)
{
	CustomParseCtx* ctx=(CustomParseCtx*)d;
	struct ParseCtx* pctx=ctx->pctx;
	if(ctx->state==PARSE_REDUCING)
	{
		FklVMvalue* ast=fklPopTopValue(exe);
		fklPushPtrStack(create_nonterm_analyzing_symbol(pctx->reducing_sid,ast),&pctx->symbolStack);
		ctx->state=PARSE_CONTINUE;
	}
	FKL_DECL_VM_UD_DATA(g,FklGrammer,ctx->parser);
	FklString* str=FKL_VM_STR(ctx->str);
	int err=0;
	uint64_t errLine=0;
	int accept=0;
	FklGrammerMatchOuterCtx outerCtx=FKL_VMVALUE_PARSE_OUTER_CTX_INIT(exe);
	size_t restLen=str->size-pctx->offset;

	parse_with_custom_parser_for_char_buf(g
			,str->str+pctx->offset
			,restLen
			,&restLen
			,&outerCtx
			,&err
			,&errLine
			,&pctx->symbolStack
			,&pctx->lineStack
			,&pctx->stateStack
			,exe
			,&accept
			,&ctx->state
			,&pctx->reducing_sid);

	if(err)
	{
		if(err==FKL_PARSE_WAITING_FOR_MORE||(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&!restLen))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF,exe);
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR,exe);
	}
	if(accept)
	{
		FklAnalysisSymbol* top=fklPopPtrStack(&pctx->symbolStack);
		FKL_VM_PUSH_VALUE(exe,top->ast);
		free(top);
		if(ctx->box)
		{
			uint64_t offset=pctx->offset=str->size-restLen;
			if(offset>FKL_FIX_INT_MAX)
				FKL_VM_BOX(ctx->box)=fklCreateVMvalueBigIntWithU64(exe,offset);
			else
				FKL_VM_BOX(ctx->box)=FKL_MAKE_VM_FIX(offset);
		}
		return 0;
	}
	else
		pctx->offset=str->size-restLen;
	return 1;
}

static inline void init_custom_parse_ctx(void* data
		,FklSid_t sid
		,FklVMvalue* parser
		,FklVMvalue* str
		,FklVMvalue* box)
{
	CustomParseCtx* ctx=(CustomParseCtx*)data;
	ctx->box=box;
	ctx->parser=parser;
	ctx->str=str;
	struct ParseCtx* pctx=create_read_parse_ctx();
	ctx->pctx=pctx;
	push_state0_of_custom_parser(parser,&pctx->stateStack);
	ctx->state=PARSE_CONTINUE;
}

static inline void custom_parse_frame_print_backtrace(void* d,FILE* fp,FklVMgc* gc)
{
	fklDoPrintCprocBacktrace(((CustomParseCtx*)d)->sid,fp,gc);
}

static const FklVMframeContextMethodTable CustomParseContextMethodTable=
{
	.atomic=custom_parse_frame_atomic,
	.finalizer=custom_parse_frame_finalizer,
	.print_backtrace=custom_parse_frame_print_backtrace,
	.step=custom_parse_frame_step,
};

static inline void init_custom_parse_frame(FklVM* exe
		,FklSid_t sid
		,FklVMvalue* parser
		,FklVMvalue* str
		,FklVMvalue* box)
{
	FklVMframe* f=exe->top_frame;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&CustomParseContextMethodTable;
	init_custom_parse_ctx(f->data,sid,parser,str,box);
}

#define CHECK_FP_READABLE(V,E) if(!isVMfpReadable(V))\
	FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_WRITABLE(V,E) if(!isVMfpWritable(V))\
	FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNSUPPORTED_OP,E)

#define CHECK_FP_OPEN(V,E) if(!FKL_VM_FP(V)->fp)\
	FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,E)

#define GET_OR_USE_STDOUT(VN) if(!VN)\
	VN=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->sysOut

#define GET_OR_USE_STDIN(VN) if(!VN)\
	VN=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->sysIn

static int builtin_read(FKL_CPROC_ARGL)
{
	FklVMvalue* stream=FKL_VM_POP_ARG(exe);
	FklVMvalue* parser=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if((stream&&!FKL_IS_FP(stream))||(parser&&!is_custom_parser(parser)))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(!stream||FKL_IS_FP(stream))
	{
		FKL_DECL_VM_UD_DATA(pbd,PublicBuiltInData,ctx->pd);
		FklVMvalue* fpv=stream?stream:pbd->sysIn;
		CHECK_FP_READABLE(fpv,exe);
		CHECK_FP_OPEN(fpv,exe);
		if(parser)
			init_custom_read_frame(exe,FKL_VM_CPROC(ctx->proc)->sid,parser,stream);
		else
			initFrameToReadFrame(exe,FKL_VM_CPROC(ctx->proc)->sid,fpv);
	}
	return 1;
}

static int builtin_stringify(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(v,exe);
	FklVMvalue* is_princ=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* retval=NULL;
	if(is_princ&&is_princ!=FKL_VM_NIL)
		retval=fklCreateVMvalueStr(exe,fklVMstringifyAsPrinc(v,exe->gc));
	else
		retval=fklCreateVMvalueStr(exe,fklVMstringify(v,exe->gc));
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_parse(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(str,exe);
	if(!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

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
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	next_arg=FKL_VM_POP_ARG(exe);
	if(next_arg)
	{
		if(FKL_IS_BOX(next_arg)&&!box)
			box=next_arg;
		else if(is_custom_parser(next_arg)&&!custom_parser)
			custom_parser=next_arg;
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}

	FKL_CHECK_REST_ARG(exe);

	if(custom_parser)
	{
		FKL_CHECK_TYPE(custom_parser,is_custom_parser,exe);
		init_custom_parse_frame(exe,FKL_VM_CPROC(ctx->proc)->sid,custom_parser,str,box);
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
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR,exe);
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

static inline FklVMvalue* vm_fgetc(FklVM* exe,FILE* fp)
{
	fklUnlockThread(exe);
	int ch=fgetc(fp);
	fklLockThread(exe);
	if(ch==EOF)
		return FKL_VM_NIL;
	return FKL_MAKE_VM_CHR(ch);
}

static int builtin_fgetc(FKL_CPROC_ARGL)
{
	FklVMvalue* stream=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDIN(stream);
	FKL_CHECK_TYPE(stream,FKL_IS_FP,exe);
	CHECK_FP_OPEN(stream,exe);
	CHECK_FP_READABLE(stream,exe);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,stream);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,vm_fgetc(exe,FKL_VM_FP(stream)->fp));
	return 0;
}

static inline FklVMvalue* vm_fgeti(FklVM* exe,FILE* fp)
{
	fklUnlockThread(exe);
	int ch=fgetc(fp);
	fklLockThread(exe);
	if(ch==EOF)
		return FKL_VM_NIL;
	return FKL_MAKE_VM_FIX(ch);
}

static int builtin_fgeti(FKL_CPROC_ARGL)
{
	FklVMvalue* stream=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDIN(stream);
	FKL_CHECK_TYPE(stream,FKL_IS_FP,exe);
	CHECK_FP_OPEN(stream,exe);
	CHECK_FP_READABLE(stream,exe);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,stream);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,vm_fgeti(exe,FKL_VM_FP(stream)->fp));
	return 0;
}

static int builtin_fgetd(FKL_CPROC_ARGL)
{
	FklVMvalue* del=FKL_VM_POP_ARG(exe);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	int d=EOF;
	if(del)
	{
		FKL_CHECK_TYPE(del,FKL_IS_CHR,exe);
		d=FKL_GET_CHR(del);
	}
	else
		d='\n';
	GET_OR_USE_STDIN(file);
	FKL_CHECK_TYPE(file,FKL_IS_FP,exe);
	CHECK_FP_OPEN(file,exe);
	CHECK_FP_READABLE(file,exe);

	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,file);
	FklStringBuffer buf;
	fklInitStringBufferWithCapacity(&buf,80);
	fklVMread(exe,FKL_VM_FP(file)->fp,&buf,80,d);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklStringBufferToString(&buf));
	fklUninitStringBuffer(&buf);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,r);
	return 0;
}

static int builtin_fgets(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(psize,exe);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!fklIsVMint(psize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	CHECK_FP_OPEN(file,exe);
	CHECK_FP_READABLE(file,exe);

	uint64_t len=fklGetUint(psize);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,file);
	FklStringBuffer buf;
	fklInitStringBufferWithCapacity(&buf,len);
	fklVMread(exe,FKL_VM_FP(file)->fp,&buf,len,EOF);
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklStringBufferToString(&buf));
	fklUninitStringBuffer(&buf);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,r);
	return 0;
}

static int builtin_fgetb(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(psize,exe);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDIN(file);
	if(!FKL_IS_FP(file)||!fklIsVMint(psize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(psize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	CHECK_FP_OPEN(file,exe);
	CHECK_FP_READABLE(file,exe);
	
	uint64_t len=fklGetUint(psize);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,file);

	FklStringBuffer buf;
	fklInitStringBufferWithCapacity(&buf,len);
	fklVMread(exe,FKL_VM_FP(file)->fp,&buf,len,EOF);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklStringBufferToBytevector(&buf));
	fklUninitStringBuffer(&buf);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,r);
	return 0;
}

static int builtin_prin1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDOUT(file);
	if(!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	CHECK_FP_OPEN(file,exe);
	CHECK_FP_WRITABLE(file,exe);

	FILE* fp=FKL_VM_FP(file)->fp;
	fklPrin1VMvalue(obj,fp,exe->gc);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_princ(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDOUT(file);
	if(!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	CHECK_FP_OPEN(file,exe);
	CHECK_FP_WRITABLE(file,exe);

	FILE* fp=FKL_VM_FP(file)->fp;
	fklPrincVMvalue(obj,fp,exe->gc);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_println(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrincVMvalue(obj,stdout,exe->gc);
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
		fklPrincVMvalue(obj,stdout,exe->gc);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_printf(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(fmt_obj,exe);
	FKL_CHECK_TYPE(fmt_obj,FKL_IS_STR,exe);

	uint64_t len=0;
	FklBuiltinErrorType err_type=fklVMprintf(exe,stdout,FKL_VM_STR(fmt_obj),&len);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);

	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_format(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(fmt_obj,exe);
	FKL_CHECK_TYPE(fmt_obj,FKL_IS_STR,exe);

	uint64_t len=0;
	FklStringBuffer buf;
	fklInitStringBuffer(&buf);
	FklBuiltinErrorType err_type=fklVMformat(exe,&buf,FKL_VM_STR(fmt_obj),&len);
	if(err_type)
	{
		fklUninitStringBuffer(&buf);
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	}

	FklVMvalue* str_value=fklCreateVMvalueStr(exe,fklStringBufferToString(&buf));
	fklUninitStringBuffer(&buf);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,str_value);
	return 0;
}

static int builtin_prin1n(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrin1VMvalue(obj,stdout,exe->gc);
	fputc('\n',stdout);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_prin1v(FKL_CPROC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=FKL_VM_NIL;
	for(;obj;r=obj,obj=FKL_VM_POP_ARG(exe))
		fklPrin1VMvalue(obj,stdout,exe->gc);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_newline(FKL_CPROC_ARGL)
{
	FklVMvalue* file=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	GET_OR_USE_STDOUT(file);
	FKL_CHECK_TYPE(file,FKL_IS_FP,exe);
	CHECK_FP_OPEN(file,exe);
	CHECK_FP_WRITABLE(file,exe);
	FILE* fp=FKL_VM_FP(file)->fp;
	fputc('\n',fp);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_dlopen(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(dllName,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(dllName,FKL_IS_STR,exe);
	FklVMvalue* errorStr=NULL;
	FklVMvalue* ndll=fklCreateVMvalueDll(exe,FKL_VM_STR(dllName)->str,&errorStr);
	if(!ndll)
		FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_LOADDLLFAILD,exe,FKL_VM_STR(errorStr)->str);
	fklInitVMdll(ndll,exe);
	FKL_VM_PUSH_VALUE(exe,ndll);
	return 0;
}

static int builtin_dlsym(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(ndll,symbol,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(ndll))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMdll* dll=FKL_VM_DLL(ndll);
	FklVMcFunc funcAddress=NULL;
	if(uv_dlsym(&dll->dll,FKL_VM_STR(symbol)->str,(void**)&funcAddress))
		FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_LOADDLLFAILD,exe,uv_dlerror(&dll->dll));
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueCproc(exe,funcAddress,ndll,dll->pd,0));
	return 0;
}

static int builtin_argv(FKL_CPROC_ARGL)
{
	FklVMvalue* retval=NULL;
	FKL_CHECK_REST_ARG(exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=exe->gc->argc;
	char* const* const argv=exe->gc->argv;
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

static inline int match_pattern(const FklVMvalue* pattern,FklVMvalue* exp,FklHashTable* ht,FklVMgc* gc)
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
		else if(FKL_IS_DVECTOR(v0)
				&&FKL_IS_DVECTOR(v1))
		{
			FklVMdvec* vec0=FKL_VM_DVEC(v0);
			FklVMdvec* vec1=FKL_VM_DVEC(v1);
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
	FKL_DECL_AND_CHECK_ARG2(pattern,exp,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!isValidSyntaxPattern(pattern))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDPATTERN,exe);
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* hash=FKL_VM_HASH(r);
	if(match_pattern(pattern,exp,hash,exe->gc))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_go(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(threadProc,exe);
	if(!fklIsCallable(threadProc))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVM* threadVM=fklCreateThreadVM(threadProc
			,exe
			,exe->next
			,exe->libNum
			,exe->libs);
	fklSetBp(threadVM);
	size_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	if(arg_num)
	{
		FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);
		for(uint32_t i=0;i<arg_num;i++)
			FKL_VM_PUSH_VALUE(threadVM,base[i]);
		exe->tp-=arg_num;
	}
	fklResBp(exe);
	FklVMvalue* chan=threadVM->chan;
	FKL_VM_PUSH_VALUE(exe,chan); 
	fklVMthreadStart(threadVM,&exe->gc->q);
	return 0;
}

static int builtin_chanl(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(maxSize,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(maxSize,fklIsVMint,exe);
	if(fklIsVMnumberLt0(maxSize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueChanl(exe,fklGetUint(maxSize)));
	return 0;
}

static int builtin_chanl_msg_num(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklVMchanlMessageNum(FKL_VM_CHANL(obj));
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_chanl_send_num(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklVMchanlSendqLen(FKL_VM_CHANL(obj));
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_chanl_recv_num(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	size_t len=0;
	if(FKL_IS_CHAN(obj))
		len=fklVMchanlRecvqLen(FKL_VM_CHANL(obj));
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(len,exe));
	return 0;
}

static int builtin_chanl_full_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* retval=NULL;
	if(FKL_IS_CHAN(obj))
		retval=fklVMchanlFull(FKL_VM_CHANL(obj))?FKL_VM_TRUE:FKL_VM_NIL;
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_chanl_empty_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* retval=NULL;
	if(FKL_IS_CHAN(obj))
		retval=fklVMchanlEmpty(FKL_VM_CHANL(obj))?FKL_VM_TRUE:FKL_VM_NIL;
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_chanl_msg_to_list(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMvalue* r=NULL;
	FklVMvalue** cur=&r;
	if(FKL_IS_CHAN(obj))
	{
		FklVMchanl* ch=FKL_VM_CHANL(obj);
		uv_mutex_lock(&ch->lock);
		if(ch->recvx<ch->sendx)
		{

			FklVMvalue** const end=&ch->buf[ch->sendx];
			for(FklVMvalue** buf=&ch->buf[ch->recvx];buf<end;buf++)
			{
				*cur=fklCreateVMvaluePairWithCar(exe,*buf);
				cur=&FKL_VM_CDR(*cur);
			}
		}
		else
		{
			FklVMvalue** end=&ch->buf[ch->qsize];
			FklVMvalue** buf=&ch->buf[ch->recvx];
			for(;buf<end;buf++)
			{
				*cur=fklCreateVMvaluePairWithCar(exe,*buf);
				cur=&FKL_VM_CDR(*cur);
			}

			buf=ch->buf;
			end=&ch->buf[ch->sendx];
			for(;buf<end;buf++)
			{
				*cur=fklCreateVMvaluePairWithCar(exe,*buf);
				cur=&FKL_VM_CDR(*cur);
			}
		}
		uv_mutex_unlock(&ch->lock);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}



static int builtin_send(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(ch,message,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,exe);
	uint32_t rtp=exe->tp;
	FKL_VM_PUSH_VALUE(exe,message);
	FKL_VM_PUSH_VALUE(exe,ch);
	fklChanlSend(FKL_VM_CHANL(ch),message,exe);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,message);
	return 0;
}

static int builtin_recv(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(ch,exe);
	FklVMvalue* okBox=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,exe);
	FklVMchanl* chanl=FKL_VM_CHANL(ch);
	if(okBox)
	{
		FKL_CHECK_TYPE(okBox,FKL_IS_BOX,exe);
		FklVMvalue* r=FKL_VM_NIL;
		FKL_VM_BOX(okBox)=fklChanlRecvOk(chanl,&r)?FKL_VM_TRUE:FKL_VM_NIL;
		FKL_VM_PUSH_VALUE(exe,r);
	}
	else
	{
		uint32_t rtp=exe->tp;
		FKL_VM_PUSH_VALUE(exe,ch);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		fklChanlRecv(chanl,&FKL_VM_GET_VALUE(exe,1),exe);
		FklVMvalue* r=FKL_VM_GET_VALUE(exe,1);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,r);
	}
	return 0;
}

static int builtin_recv7(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(ch,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ch,FKL_IS_CHAN,exe);
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
	FKL_DECL_AND_CHECK_ARG2(type,message,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,fklCreateVMvalueError(exe
				,FKL_GET_SYM(type)
				,fklCopyString(FKL_VM_STR(message))));
	return 0;
}

static int builtin_error_type(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(err,exe);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMerror* error=FKL_VM_ERR(err);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(error->type));
	return 0;
}

static int builtin_error_msg(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(err,exe);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMerror* error=FKL_VM_ERR(err);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCopyString(error->message)));
	return 0;
}

static int builtin_raise(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(err,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(err,FKL_IS_ERR,exe);
	fklPopVMframe(exe);
	fklRaiseVMerror(err,exe);
	return 0;
}

static int builtin_throw(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(type,message,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* err=fklCreateVMvalueError(exe
			,FKL_GET_SYM(type)
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
	uint32_t tp;
	uint32_t bp;
}EhFrameContext;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(EhFrameContext);

static void error_handler_frame_print_backtrace(void* data,FILE* fp,FklVMgc* gc)
{
	EhFrameContext* c=(EhFrameContext*)data;
	FklVMcproc* cproc=FKL_VM_CPROC(c->proc);
	if(cproc->sid)
	{
		fprintf(fp,"at cproc: ");
		fklPrintRawSymbol(fklVMgetSymbolWithId(gc,cproc->sid)->symbol
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
	dc->errorSymbolLists=(FklVMvalue**)malloc(num*sizeof(FklVMvalue*));
	FKL_ASSERT(dc->errorSymbolLists||!num);
	dc->errorHandlers=(FklVMvalue**)malloc(num*sizeof(FklVMvalue*));
	FKL_ASSERT(dc->errorHandlers||!num);
	for(size_t i=0;i<num;i++)
	{
		dc->errorSymbolLists[i]=sc->errorSymbolLists[i];
		dc->errorHandlers[i]=sc->errorHandlers[i];
	}
}

static int error_handler_frame_step(void* data,FklVM* exe)
{
	return 0;
}

static const FklVMframeContextMethodTable ErrorHandlerContextMethodTable=
{
	.atomic=error_handler_frame_atomic,
	.finalizer=error_handler_frame_finalizer,
	.copy=error_handler_frame_copy,
	.print_backtrace=error_handler_frame_print_backtrace,
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
			exe->bp=c->bp;
			exe->tp=c->tp;
			fklSetBp(exe);
			FKL_VM_PUSH_VALUE(exe,errValue);
			FklVMframe* topFrame=exe->top_frame;
			exe->top_frame=f;
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

static inline int is_symbol_list(const FklVMvalue* p)
{
	for(;p!=FKL_VM_NIL;p=FKL_VM_CDR(p))
		if(!FKL_IS_PAIR(p)||!FKL_IS_SYM(FKL_VM_CAR(p)))
			return 0;
	return 1;
}

static int builtin_call_eh(FKL_CPROC_ARGL)
{
#define GET_LIST (0)
#define GET_PROC (1)

	FKL_DECL_AND_CHECK_ARG(proc,exe);
	FKL_CHECK_TYPE(proc,fklIsCallable,exe);
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
				if(!is_symbol_list(v))
				{
					fklUninitPtrStack(&errSymbolLists);
					fklUninitPtrStack(&errHandlers);
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE
							,exe);
				}
				fklPushPtrStack(v,&errSymbolLists);
				break;
			case GET_PROC:
				if(!fklIsCallable(v))
				{
					fklUninitPtrStack(&errSymbolLists);
					fklUninitPtrStack(&errHandlers);
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE
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
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	}
	fklResBp(exe);
	if(errSymbolLists.top)
	{
		FklVMframe* top_frame=exe->top_frame;
		top_frame->errorCallBack=errorCallBackWithErrorHandler;
		top_frame->t=&ErrorHandlerContextMethodTable;
		EhFrameContext* c=(EhFrameContext*)top_frame->data;
		c->num=errSymbolLists.top;
		FklVMvalue** t=(FklVMvalue**)fklRealloc(errSymbolLists.base,errSymbolLists.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorSymbolLists=t;
		t=(FklVMvalue**)fklRealloc(errHandlers.base,errHandlers.top*sizeof(FklVMvalue*));
		FKL_ASSERT(t);
		c->errorHandlers=t;
		c->tp=exe->tp;
		c->bp=exe->bp;
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

static int pcall_error_handler(FklVMframe* f,FklVMvalue* errValue,FklVM* exe)
{
	while(exe->top_frame!=f)
		fklPopVMframe(exe);
	FklCprocFrameContext* ctx=(FklCprocFrameContext*)f->data;
	exe->tp=ctx->rtp;
	exe->bp=ctx->cbu;
	ctx->cau=2;
	FKL_VM_PUSH_VALUE(exe,errValue);
	return 1;
}

static int builtin_pcall(FKL_CPROC_ARGL)
{
	switch(ctx->cau)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG(proc,exe);
				FKL_CHECK_TYPE(proc,fklIsCallable,exe);
				uint32_t cbp=exe->bp;
				ctx->rtp=fklResBpIn(exe,FKL_VM_GET_ARG_NUM(exe));
				ctx->cbu=exe->bp;
				exe->bp=cbp;
				exe->top_frame->errorCallBack=pcall_error_handler;
				fklCallObj(exe,proc);
				ctx->cau=1;
				return 1;
			}
			break;
		case 1:
			{
				FklVMvalue* top=FKL_VM_POP_ARG(exe);
				FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,top));
			}
			break;
	}
	return 0;
}

static void idle_queue_work_cb(FklVM* exe,void* a)
{
	FklCprocFrameContext* ctx=(FklCprocFrameContext*)a;
	fklDontNoticeThreadLock(exe);
	exe->state=FKL_VM_READY;
	fklSetVMsingleThread(exe);
	ctx->context=fklRunVMinSingleThread(exe,exe->top_frame->prev);
	exe->state=FKL_VM_READY;
	fklNoticeThreadLock(exe);
	fklUnsetVMsingleThread(exe);
	return;
}

static int builtin_idle(FKL_CPROC_ARGL)
{
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG(proc,exe);
				FKL_CHECK_TYPE(proc,fklIsCallable,exe);
				FklVMframe* origin_top_frame=exe->top_frame;
				fklCallObj(exe,proc);
				fklQueueWorkInIdleThread(exe,idle_queue_work_cb,ctx);
				if(ctx->context)
				{
					ctx->context=(uintptr_t)fklMoveVMframeToTop(exe,origin_top_frame);
					return 1;
				}
				return 0;
			}
		default:
			{
				FklVMframe* frame=(FklVMframe*)ctx->context;
				fklInsertTopVMframeAsPrev(exe,frame);
				fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			}
			break;
	}
	return 0;
}

struct AtExitArg
{
	FklVMvalue* proc;
	uint32_t arg_num;
	FklVMvalue* args[];
};

static void vm_atexit_idle_queue_work_cb(FklVM* exe,void *a)
{
	struct AtExitArg* arg=(struct AtExitArg*)a;
	fklDontNoticeThreadLock(exe);
	FklVMframe* origin_top_frame=exe->top_frame;
	uint32_t tp=exe->tp;
	uint32_t bp=exe->bp;
	fklSetBp(exe);
	for(uint32_t i=0;i<arg->arg_num;i++)
		FKL_VM_PUSH_VALUE(exe,arg->args[i]);
	fklCallObj(exe,arg->proc);
	exe->state=FKL_VM_READY;
	fklSetVMsingleThread(exe);
	if(fklRunVMinSingleThread(exe,origin_top_frame))
		fklPrintErrBacktrace(FKL_VM_POP_TOP_VALUE(exe),exe,stderr);
	while(exe->top_frame!=origin_top_frame)
		fklPopVMframe(exe);
	exe->state=FKL_VM_READY;
	fklNoticeThreadLock(exe);
	fklUnsetVMsingleThread(exe);
	exe->bp=bp;
	exe->tp=tp;
	return;
}

static void vm_atexit_func(FklVM* vm,void* a)
{
	fklQueueWorkInIdleThread(vm,vm_atexit_idle_queue_work_cb,a);
}

static void vm_atexit_mark(void* a,FklVMgc* gc)
{
	const struct AtExitArg* arg=(const struct AtExitArg*)a;
	fklVMgcToGray(arg->proc,gc);
	for(uint32_t i=0;i<arg->arg_num;i++)
		fklVMgcToGray(arg->args[i],gc);
}

static void vm_atexit_finalizer(void* arg)
{
	free(arg);
}

static inline struct AtExitArg* create_at_exit_arg(FklVMvalue* proc
		,uint32_t arg_num
		,FklVMvalue** base)
{
	size_t size=arg_num*sizeof(FklVMvalue*);
	struct AtExitArg* r=(struct AtExitArg*)calloc(1,sizeof(struct AtExitArg)+size);
	FKL_ASSERT(r);
	r->proc=proc;
	r->arg_num=arg_num;
	memcpy(r->args,base,size);
	return r;
}

static int builtin_atexit(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(proc,exe);
	FKL_CHECK_TYPE(proc,fklIsCallable,exe);
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);
	struct AtExitArg* arg=create_at_exit_arg(proc,arg_num,base);
	fklVMatExit(exe,vm_atexit_func,vm_atexit_mark,vm_atexit_finalizer,arg);
	uint32_t rtp=fklResBpIn(exe,arg_num);
	FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,proc);
	return 0;
}

static int builtin_apply(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(proc,exe);
	FKL_CHECK_TYPE(proc,fklIsCallable,exe);
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
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	FklPtrStack stack2=FKL_STACK_INIT;
	fklInitPtrStack(&stack2,32,16);
	if(!FKL_IS_PAIR(lastList)&&lastList!=FKL_VM_NIL)
	{
		fklUninitPtrStack(&stack1);
		fklUninitPtrStack(&stack2);
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=FKL_VM_CDR(lastList))
		fklPushPtrStack(FKL_VM_CAR(lastList),&stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklUninitPtrStack(&stack1);
		fklUninitPtrStack(&stack2);
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
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
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG(proc,exe);
				FKL_CHECK_TYPE(proc,fklIsCallable,exe);
				size_t arg_num=FKL_VM_GET_ARG_NUM(exe);
				if(arg_num==0)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
				FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);
				uint32_t rtp=fklResBpIn(exe,arg_num);
				FKL_CHECK_TYPE(base[0],fklIsList,exe);
				size_t len=fklVMlistLength(base[0]);
				for(size_t i=1;i<arg_num;i++)
				{
					FKL_CHECK_TYPE(base[i],fklIsList,exe);
					if(fklVMlistLength(base[i])!=len)
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_LIST_DIFFER_IN_LENGTH,exe);
				}
				if(len==0)
				{
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
					return 0;
				}

				FklVMvalue* resultBox=fklCreateVMvalueBox(exe,FKL_VM_NIL);
				ctx->context=(uintptr_t)&FKL_VM_BOX(resultBox);
				ctx->rtp=rtp;
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

#define AND_OR_MAP_PATTERN(DEFAULT,EXIT) switch(ctx->context)\
	{\
		case 0:\
			{\
				FKL_DECL_AND_CHECK_ARG(proc,exe);\
				FKL_CHECK_TYPE(proc,fklIsCallable,exe);\
				size_t arg_num=FKL_VM_GET_ARG_NUM(exe);\
				if(arg_num==0)\
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);\
				FklVMvalue** base=&FKL_VM_GET_VALUE(exe,arg_num);\
				uint32_t rtp=fklResBpIn(exe,arg_num);\
				fklResBp(exe);\
				FKL_CHECK_TYPE(base[0],fklIsList,exe);\
				size_t len=fklVMlistLength(base[0]);\
				for(size_t i=1;i<arg_num;i++)\
				{\
					FKL_CHECK_TYPE(base[i],fklIsList,exe);\
					if(fklVMlistLength(base[i])!=len)\
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_LIST_DIFFER_IN_LENGTH,exe);\
				}\
				if(len==0)\
				{\
					FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,DEFAULT);\
					return 0;\
				}\
				ctx->context=1;\
				ctx->rtp=rtp;\
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
	AND_OR_MAP_PATTERN(FKL_VM_NIL,);
}

static int builtin_andmap(FKL_CPROC_ARGL)
{
	AND_OR_MAP_PATTERN(FKL_VM_TRUE
			,if(r==FKL_VM_NIL)
			{
			FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,r);
			return 0;
			});
}

static int builtin_ormap(FKL_CPROC_ARGL)
{
	AND_OR_MAP_PATTERN(FKL_VM_NIL
			,if(r!=FKL_VM_NIL)
			{
			FKL_VM_SET_TP_AND_PUSH_VALUE(exe,ctx->rtp,r);
			return 0;
			});
}

#undef AND_OR_MAP_PATTERN

static int builtin_memq(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,list,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(list,fklIsList,exe);
	FklVMvalue* r=list;
	for(;r!=FKL_VM_NIL;r=FKL_VM_CDR(r))
		if(FKL_VM_CAR(r)==obj)
			break;
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_member(FKL_CPROC_ARGL)
{
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG2(obj,list,exe);
				FklVMvalue* proc=FKL_VM_POP_ARG(exe);
				FKL_CHECK_REST_ARG(exe);
				FKL_CHECK_TYPE(list,fklIsList,exe);
				if(proc)
				{
					FKL_CHECK_TYPE(proc,fklIsCallable,exe);
					if(list==FKL_VM_NIL)
					{
						FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
						return 0;
					}
					ctx->context=1;
					ctx->rtp=exe->tp;
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
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG2(proc,list,exe);
				FKL_CHECK_REST_ARG(exe);
				FKL_CHECK_TYPE(proc,fklIsCallable,exe);
				FKL_CHECK_TYPE(list,fklIsList,exe);

				if(list==FKL_VM_NIL)
				{
					FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
					return 0;
				}
				ctx->context=1;
				ctx->rtp=exe->tp;
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
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG2(proc,list,exe);
				FKL_CHECK_REST_ARG(exe);
				FKL_CHECK_TYPE(proc,fklIsCallable,exe);
				FKL_CHECK_TYPE(list,fklIsList,exe);
				if(list==FKL_VM_NIL)
				{
					FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
					return 0;
				}
				FklVMvalue* resultBox=fklCreateVMvalueBoxNil(exe);
				ctx->context=(uintptr_t)&FKL_VM_BOX(resultBox);
				ctx->rtp=exe->tp;
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

static int builtin_remq1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,list,exe);
	FKL_CHECK_REST_ARG(exe);

	FKL_CHECK_TYPE(list,fklIsList,exe);
	if(list==FKL_VM_NIL)
	{
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return 0;
	}

	FklVMvalue* r=list;
	FklVMvalue** pr=&r;

	FklVMvalue* cur_pair=*pr;
	while(FKL_IS_PAIR(cur_pair))
	{
		FklVMvalue* cur=FKL_VM_CAR(cur_pair);
		if(fklVMvalueEq(obj,cur))
			*pr=FKL_VM_CDR(cur_pair);
		else
			pr=&FKL_VM_CDR(cur_pair);
		cur_pair=*pr;
	}
	FKL_VM_PUSH_VALUE(exe,r);

	return 0;
}

static int builtin_remv1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,list,exe);
	FKL_CHECK_REST_ARG(exe);

	FKL_CHECK_TYPE(list,fklIsList,exe);
	if(list==FKL_VM_NIL)
	{
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return 0;
	}

	FklVMvalue* r=list;
	FklVMvalue** pr=&r;

	FklVMvalue* cur_pair=*pr;
	while(FKL_IS_PAIR(cur_pair))
	{
		FklVMvalue* cur=FKL_VM_CAR(cur_pair);
		if(fklVMvalueEqv(obj,cur))
			*pr=FKL_VM_CDR(cur_pair);
		else
			pr=&FKL_VM_CDR(cur_pair);
		cur_pair=*pr;
	}
	FKL_VM_PUSH_VALUE(exe,r);

	return 0;
}

static int builtin_remove1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(obj,list,exe);
	FKL_CHECK_REST_ARG(exe);

	FKL_CHECK_TYPE(list,fklIsList,exe);
	if(list==FKL_VM_NIL)
	{
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return 0;
	}

	FklVMvalue* r=list;
	FklVMvalue** pr=&r;

	FklVMvalue* cur_pair=*pr;
	while(FKL_IS_PAIR(cur_pair))
	{
		FklVMvalue* cur=FKL_VM_CAR(cur_pair);
		if(fklVMvalueEqual(obj,cur))
			*pr=FKL_VM_CDR(cur_pair);
		else
			pr=&FKL_VM_CDR(cur_pair);
		cur_pair=*pr;
	}
	FKL_VM_PUSH_VALUE(exe,r);

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
	FKL_DECL_AND_CHECK_ARG(r,exe);
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
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsList,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=FKL_VM_CDR(cdr))
		retval=fklCreateVMvaluePair(exe,FKL_VM_CAR(cdr),retval);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int builtin_reverse1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsList,exe);
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
	FKL_DECL_AND_CHECK_ARG(fp,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,exe);
	CHECK_FP_OPEN(fp,exe);
	FKL_VM_PUSH_VALUE(exe,feof(FKL_VM_FP(fp)->fp)?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int builtin_ftell(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(fp,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(fp,FKL_IS_FP,exe);
	CHECK_FP_OPEN(fp,exe);
	int64_t pos=ftell(FKL_VM_FP(fp)->fp);
	if(pos<0)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMvalue* r=pos>FKL_FIX_INT_MAX
		?fklCreateVMvalueBigIntWithI64(exe,pos)
		:FKL_MAKE_VM_FIX(pos);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_fseek(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vfp,offset,exe);
	FklVMvalue* whence_v=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vfp,FKL_IS_FP,exe);
	FKL_CHECK_TYPE(offset,fklIsVMint,exe);
	if(whence_v)
		FKL_CHECK_TYPE(whence_v,FKL_IS_SYM,exe);
	CHECK_FP_OPEN(vfp,exe);
	FILE* fp=FKL_VM_FP(vfp)->fp;
	const FklSid_t seek_set=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->seek_set;
	const FklSid_t seek_cur=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->seek_cur;
	const FklSid_t seek_end=FKL_GET_UD_DATA(PublicBuiltInData,FKL_VM_UD(ctx->pd))->seek_end;
	const FklSid_t whence_sid=whence_v?FKL_GET_SYM(whence_v):seek_set;
	int whence=whence_sid==seek_set?0
		:whence_sid==seek_cur?1
		:whence_sid==seek_end?2:-1;

	if(whence==-1)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	if(fseek(fp,fklGetInt(offset),whence))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
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

static int builtin_box(FKL_CPROC_ARGL)
{
	DECL_AND_SET_DEFAULT(obj,FKL_VM_NIL);
	FKL_CHECK_REST_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,obj));
	return 0;
}

static int builtin_unbox(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(box,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_BOX(box));
	return 0;
}

static int builtin_box_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(box,obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,exe);
	FKL_VM_BOX(box)=obj;
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_box_cas(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(box,old,new,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,exe);
	if(FKL_VM_BOX_CAS(box,&old,new))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
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
		FKL_CHECK_TYPE(cur,fklIsVMint,exe);
		bytevec->ptr[i]=fklGetInt(cur);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_bytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(size,exe);
	FKL_CHECK_TYPE(size,fklIsVMint,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(fklIsVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(len,NULL));
	FklBytevector* bytevec=FKL_VM_BVEC(r);
	uint8_t u_8=0;
	if(content)
	{
		FKL_CHECK_TYPE(content,fklIsVMint,exe);
		u_8=fklGetUint(content);
	}
	memset(bytevec->ptr,u_8,len);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

#define PREDICATE(condition) FKL_DECL_AND_CHECK_ARG(val,exe);\
	FKL_CHECK_REST_ARG(exe);\
	FKL_VM_PUSH_VALUE(exe,(condition)\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;

static int builtin_sleep(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(second,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(second,fklIsVMint,exe);
	if(fklIsVMnumberLt0(second))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t sec=fklGetUint(second);
	fklVMsleep(exe,sec*1000);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_msleep(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(second,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(second,fklIsVMint,exe);
	if(fklIsVMnumberLt0(second))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t ms=fklGetUint(second);
	fklVMsleep(exe,ms);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_hash(FKL_CPROC_ARGL)
{
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_hash_num(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(ht,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(FKL_VM_HASH(ht)->num,exe));
	return 0;
}

static int builtin_make_hash(FKL_CPROC_ARGL)
{
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		FklVMvalue* value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_hasheqv(FKL_CPROC_ARGL)
{
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_make_hasheqv(FKL_CPROC_ARGL)
{
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		FklVMvalue* value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_hashequal(FKL_CPROC_ARGL)
{
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* cur=FKL_VM_POP_ARG(exe)
			;cur
			;cur=FKL_VM_POP_ARG(exe))
	{
		if(!FKL_IS_PAIR(cur))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklVMhashTableSet(FKL_VM_CAR(cur),FKL_VM_CDR(cur),ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);;
	return 0;
}

static int builtin_make_hashequal(FKL_CPROC_ARGL)
{
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	FklHashTable* ht=FKL_VM_HASH(r);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		FklVMvalue* value=FKL_VM_POP_ARG(exe);
		if(!value)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
		fklVMhashTableSet(key,value,ht);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_hash_ref(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(ht,key,exe);
	FklVMvalue* defa=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	int ok=0;
	FklVMvalue* retval=fklVMhashTableGet(key,FKL_VM_HASH(ht),&ok);
	if(ok)
		FKL_VM_PUSH_VALUE(exe,retval);
	else
	{
		if(defa)
			FKL_VM_PUSH_VALUE(exe,defa);
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NO_VALUE_FOR_KEY,exe);
	}
	return 0;
}

static int builtin_hash_ref4(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(ht,key,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	FklVMhashTableItem* i=fklVMhashTableGetItem(key,FKL_VM_HASH(ht));
	if(i)
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,i->key,i->v));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_hash_ref7(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(ht,key,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
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
	FKL_DECL_AND_CHECK_ARG3(ht,key,toSet,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	FklVMhashTableItem* item=fklVMhashTableRef1(key,toSet,FKL_VM_HASH(ht),exe->gc);
	FKL_VM_PUSH_VALUE(exe,item->v);
	return 0;
}

static int builtin_hash_clear(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(ht,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	fklClearHashTable(FKL_VM_HASH(ht));
	FKL_VM_PUSH_VALUE(exe,ht);
	return 0;
}

static int builtin_hash_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ht,key,value,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	fklVMhashTableSet(key,value,FKL_VM_HASH(ht));
	FKL_VM_PUSH_VALUE(exe,value);;
	return 0;
}

static int builtin_hash_del1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(ht,key,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	FklVMhashTableItem i;
	if(fklDelHashItem(&key,FKL_VM_HASH(ht),&i))
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,i.key,i.v));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_hash_set8(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(ht,exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
	size_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	if(arg_num%2)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	FklVMvalue* value=NULL;
	FklHashTable* hash=FKL_VM_HASH(ht);
	for(FklVMvalue* key=FKL_VM_POP_ARG(exe);key;key=FKL_VM_POP_ARG(exe))
	{
		value=FKL_VM_POP_ARG(exe);
		fklVMhashTableSet(key,value,hash);
	}
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,(value?value:ht));
	return 0;
}

static int builtin_hash_to_list(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(ht,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
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
	FKL_DECL_AND_CHECK_ARG(ht,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
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
	FKL_DECL_AND_CHECK_ARG(ht,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ht,FKL_IS_HASHTABLE,exe);
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

static int builtin_not(FKL_CPROC_ARGL) {PREDICATE(val==FKL_VM_NIL)}
static int builtin_null(FKL_CPROC_ARGL) {PREDICATE(val==FKL_VM_NIL)}
static int builtin_atom(FKL_CPROC_ARGL) {PREDICATE(!FKL_IS_PAIR(val))}
static int builtin_char_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_CHR(val))}
static int builtin_integer_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_FIX(val)||FKL_IS_BIG_INT(val))}
static int builtin_fixint_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_FIX(val))}
static int builtin_f64_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_F64(val))}
static int builtin_number_p(FKL_CPROC_ARGL) {PREDICATE(fklIsVMnumber(val))}
static int builtin_pair_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_PAIR(val))}
static int builtin_symbol_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_SYM(val))}
static int builtin_string_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_STR(val))}
static int builtin_error_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_ERR(val))}
static int builtin_procedure_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_PROC(val)||FKL_IS_CPROC(val))}
static int builtin_proc_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_PROC(val))}
static int builtin_cproc_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_CPROC(val))}
static int builtin_vector_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_VECTOR(val))}
static int builtin_dvec_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_DVECTOR(val))}
static int builtin_bytevector_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_BYTEVECTOR(val))}
static int builtin_chanl_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_CHAN(val))}
static int builtin_dll_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_DLL(val))}
static int builtin_bigint_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_BIG_INT(val))}
static int builtin_list_p(FKL_CPROC_ARGL){PREDICATE(fklIsList(val))}
static int builtin_box_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_BOX(val))}
static int builtin_hash_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE(val))}
static int builtin_hasheq_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQ(val))}
static int builtin_hasheqv_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQV(val))}
static int builtin_hashequal_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_HASHTABLE_EQUAL(val))}

static int builtin_eof_p(FKL_CPROC_ARGL) {PREDICATE(fklIsVMeofUd(val))}

static int builtin_parser_p(FKL_CPROC_ARGL) {PREDICATE(FKL_IS_USERDATA(val)&&FKL_VM_UD(val)->t==&CustomParserMetaTable)}

static int builtin_exit(FKL_CPROC_ARGL)
{
	FklVMvalue* exit_val=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(exe->chan==NULL)
	{
		if(exit_val)
		{
			exe->gc->exit_code=exit_val==FKL_VM_NIL
				?0
				:(FKL_IS_FIX(exit_val)?FKL_GET_FIX(exit_val):255);
		}
		else
			exe->gc->exit_code=0;
	}
	exe->state=FKL_VM_EXIT;
	FKL_VM_PUSH_VALUE(exe,exit_val?exit_val:FKL_VM_NIL);
	return 0;
}

static int builtin_return(FKL_CPROC_ARGL)
{
	FklVMvalue* exit_val=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FklVMframe* frame=exe->top_frame->prev;
	if(frame&&frame->type==FKL_FRAME_COMPOUND)
	{
		exe->tp=frame->c.tp;
		exe->bp=frame->c.bp;
		frame->c.pc=frame->c.end-1;
	}
	FKL_VM_PUSH_VALUE(exe,exit_val?exit_val:FKL_VM_NIL);
	return 0;
}

static int builtin_dvec(FKL_CPROC_ARGL)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* vec=fklCreateVMvalueDvec(exe,size);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	for(size_t i=0;i<size;i++)
		v->base[i]=FKL_VM_POP_ARG(exe);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_make_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(size,exe);
	FKL_CHECK_TYPE(size,fklIsVMint,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(fklIsVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueDvec(exe,len);
	if(!content)
		content=FKL_VM_NIL;
	FklVMdvec* vec=FKL_VM_DVEC(r);
	for(size_t i=0;i<len;i++)
		vec->base[i]=content;
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_make_dvec_with_capacity(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(size,exe);
	FKL_CHECK_TYPE(size,fklIsVMint,exe);
	FKL_CHECK_REST_ARG(exe);
	if(fklIsVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklVMvalue* r=fklCreateVMvalueDvecWithCapacity(exe,len);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_subdvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ovec,vstart,vend,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ovec,FKL_IS_DVECTOR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vend,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vend))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMdvec* vec=FKL_VM_DVEC(ovec);
	size_t size=vec->size;
	size_t start=fklGetUint(vstart);
	size_t end=fklGetUint(vend);
	if(start>size||end<start||end>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=end-start;
	FklVMvalue* r=fklCreateVMvalueDvecWithPtr(exe,size,vec->base+start);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_sub_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(ovec,vstart,vsize,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(ovec,FKL_IS_DVECTOR,exe);
	FKL_CHECK_TYPE(vstart,fklIsVMint,exe);
	FKL_CHECK_TYPE(vsize,fklIsVMint,exe);
	if(fklIsVMnumberLt0(vstart)||fklIsVMnumberLt0(vsize))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMdvec* vec=FKL_VM_DVEC(ovec);
	size_t size=vec->size;
	size_t start=fklGetUint(vstart);
	size_t osize=fklGetUint(vsize);
	if(start+osize>size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	size=osize;
	FklVMvalue* r=fklCreateVMvalueDvecWithPtr(exe,size,vec->base+start);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_dvec_empty_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_DVECTOR,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_DVEC(obj)->size?FKL_VM_NIL:FKL_VM_TRUE);
	return 0;
}

static int builtin_dvec_capacity(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_DVECTOR,exe);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(FKL_VM_DVEC(obj)->capacity,exe));
	return 0;
}

static int builtin_dvec_first(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	if(!v->size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[0]);
	return 0;
}

static int builtin_dvec_last(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	if(!v->size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[v->size-1]);
	return 0;
}

static int builtin_dvec_assign(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	switch(arg_num)
	{
		case 0:
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
			break;
		case 1:
			{
				FklVMvalue* another_vec_val=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				if(FKL_IS_VECTOR(another_vec_val))
				{
					FklVMdvec* v=FKL_VM_DVEC(vec);
					FklVMvec* another_vec=FKL_VM_VEC(another_vec_val);
					dvec_reserve(v,another_vec->size);
					memcpy(v->base,another_vec->base,another_vec->size*sizeof(FklVMvalue*));
					v->size=another_vec->size;
				}
				else if(FKL_IS_DVECTOR(another_vec_val))
				{
					FklVMdvec* v=FKL_VM_DVEC(vec);
					FklVMdvec* another_vec=FKL_VM_DVEC(another_vec_val);
					dvec_reserve(v,another_vec->size);
					memcpy(v->base,another_vec->base,another_vec->size*sizeof(FklVMvalue*));
					v->size=another_vec->size;
				}
				else
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
			}
			break;
		case 2:
			{
				FklVMvalue* count_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* content=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				FKL_CHECK_TYPE(count_val,fklIsVMint,exe);
				if(fklIsVMnumberLt0(count_val))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				size_t count=fklGetUint(count_val);
				FklVMdvec* v=FKL_VM_DVEC(vec);
				dvec_reserve(v,count);
				for(size_t i=0;i<count;i++)
					v->base[i]=content;
				v->size=count;
			}
			break;
		default:
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG,exe);
			break;
	}
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_dvec_clear(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FKL_VM_DVEC(vec)->size=0;
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_dvec_reserve(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,new_cap_val,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FKL_CHECK_TYPE(new_cap_val,fklIsVMint,exe);
	if(fklIsVMnumberLt0(new_cap_val))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t new_cap=fklGetUint(new_cap_val);
	dvec_reserve(v,new_cap);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_dvec_push(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	dvec_reserve(v,v->size+1);
	v->base[v->size++]=obj;
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int builtin_dvec_pop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	if(!v->size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[--v->size]);
	return 0;
}

static int builtin_dvec_pop7(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	if(v->size)
		FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,v->base[--v->size]));
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int builtin_dvec_remove(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	switch(arg_num)
	{
		case 0:
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
			break;
		case 1:
			{
				FklVMvalue* index_val=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				FKL_CHECK_TYPE(index_val,fklIsVMint,exe);
				if(fklIsVMnumberLt0(index_val))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				size_t index=fklGetUint(index_val);
				FklVMdvec* v=FKL_VM_DVEC(vec);
				if(index>=v->size)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
				FklVMvalue* r=v->base[index];
				v->size--;
				if(index<v->size)
				{
					FklVMvalue** const end=&v->base[v->size];
					for(FklVMvalue** cur=&v->base[index];cur<end;cur++)
						*cur=cur[1];
				}
				FKL_VM_PUSH_VALUE(exe,r);
			}
			break;
		case 2:
			{
				FklVMvalue* start_index_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* size_val=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				if(fklIsVMnumberLt0(start_index_val)||fklIsVMnumberLt0(size_val))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				size_t start_index=fklGetUint(start_index_val);
				size_t size=fklGetUint(size_val);
				FklVMdvec* v=FKL_VM_DVEC(vec);
				if(start_index+size>v->size)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
				v->size-=size;
				FklVMvalue** cur=&v->base[start_index];
				FklVMvalue* r=fklCreateVMvalueVecWithPtr(exe,size,cur);
				if(start_index<v->size)
				{
					FklVMvalue** const end=&v->base[v->size];
					for(;cur<end;cur++)
						*cur=cur[size];
				}
				FKL_VM_PUSH_VALUE(exe,r);
			}
			break;
		default:
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG,exe);
			break;
	}
	return 0;
}

static int builtin_dvec_insert(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	switch(arg_num)
	{
		case 0:
		case 1:
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
			break;
		case 2:
			{
				FklVMvalue* idx_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* obj=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				FKL_CHECK_TYPE(idx_val,fklIsVMint,exe);
				if(fklIsVMnumberLt0(idx_val))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				size_t idx=fklGetUint(idx_val);
				FklVMdvec* v=FKL_VM_DVEC(vec);
				if(idx>v->size)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
				dvec_reserve(v,v->size+1);
				FklVMvalue** const end=&v->base[idx];
				for(FklVMvalue** last=&v->base[v->size];last>end;last--)
					*last=last[-1];
				v->base[idx]=obj;
				v->size++;
			}
			break;
		case 3:
			{
				FklVMvalue* idx_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* count_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* obj=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				FKL_CHECK_TYPE(idx_val,fklIsVMint,exe);
				if(fklIsVMnumberLt0(idx_val)||fklIsVMnumberLt0(count_val))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				size_t idx=fklGetUint(idx_val);
				FklVMdvec* v=FKL_VM_DVEC(vec);
				if(idx>v->size)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
				size_t count=fklGetUint(count_val);
				if(count>0)
				{
					dvec_reserve(v,v->size+count);

					FklVMvalue** const end=&v->base[idx+count-1];
					for(FklVMvalue** last=&v->base[v->size+count-1];last>end;last--)
						*last=last[-count];

					for(FklVMvalue** cur=&v->base[idx];cur<=end;cur++)
						*cur=obj;
					v->size+=count;
				}
			}
			break;
		case 4:
			{
				FklVMvalue* idx_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* another_vec_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* start_idx_val=FKL_VM_POP_ARG(exe);
				FklVMvalue* count_val=FKL_VM_POP_ARG(exe);
				fklResBp(exe);
				FKL_CHECK_TYPE(idx_val,fklIsVMint,exe);
				FKL_CHECK_TYPE(start_idx_val,fklIsVMint,exe);
				FKL_CHECK_TYPE(count_val,fklIsVMint,exe);
				if(fklIsVMnumberLt0(idx_val)||fklIsVMnumberLt0(start_idx_val)||fklIsVMnumberLt0(count_val))
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				size_t idx=fklGetUint(idx_val);
				FklVMdvec* v=FKL_VM_DVEC(vec);
				if(idx>v->size)
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
				FklVMvalue** src_mem=NULL;
				size_t start_idx=fklGetUint(start_idx_val);
				size_t count=fklGetUint(count_val);
				if(FKL_IS_VECTOR(another_vec_val))
				{
					FklVMvec* v=FKL_VM_VEC(another_vec_val);
					if(start_idx+count>v->size)
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
					src_mem=&v->base[start_idx];
				}
				else if(FKL_IS_DVECTOR(another_vec_val))
				{
					FklVMdvec* v=FKL_VM_DVEC(another_vec_val);
					if(start_idx+count>v->size)
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
					src_mem=&v->base[start_idx];
				}
				else
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
				if(count>0)
				{
					dvec_reserve(v,v->size+count);
					FklVMvalue** const end=&v->base[idx+count-1];
					for(FklVMvalue** last=&v->base[v->size+count-1];last>end;last--)
						*last=last[-count];
					memcpy(&v->base[idx],src_mem,count*sizeof(FklVMvalue*));
					v->size+=count;
				}
			}
			break;
	}
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_dvec_shrink(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FklVMvalue* shrink_to_val=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t shrink_to;
	if(shrink_to_val)
	{
		FKL_CHECK_TYPE(shrink_to_val,fklIsVMint,exe);
		if(fklIsVMnumberLt0(shrink_to_val))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		shrink_to=fklGetUint(shrink_to_val);
	}
	else
		shrink_to=v->size;
	if(shrink_to<v->capacity&&shrink_to>=v->size)
	{
		v->capacity=shrink_to;
		FklVMvalue** base=(FklVMvalue**)fklRealloc(v->base,shrink_to*sizeof(FklVMvalue*));
		FKL_ASSERT(base);
		v->base=base;
	}
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_dvec_resize(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,new_size_val,exe);
	FklVMvalue* content=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FKL_CHECK_TYPE(new_size_val,fklIsVMint,exe);
	if(fklIsVMnumberLt0(new_size_val))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t new_size=fklGetUint(new_size_val);
	if(!content)
		content=FKL_VM_NIL;
	if(new_size<v->size)
		v->size=new_size;
	else if(new_size>v->size)
	{
		dvec_reserve(v,new_size);
		FklVMvalue** const end=&v->base[new_size];
		for(FklVMvalue** cur=&v->base[v->size];cur<end;cur++)
			*cur=content;
		v->size=new_size;
	}
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_list_to_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,fklIsList,exe);
	size_t len=fklVMlistLength(obj);
	FklVMvalue* r=fklCreateVMvalueDvec(exe,len);
	FklVMdvec* vec=FKL_VM_DVEC(r);
	for(size_t i=0;obj!=FKL_VM_NIL;i++,obj=FKL_VM_CDR(obj))
		vec->base[i]=FKL_VM_CAR(obj);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_string_to_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe)
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_STR,exe);
	FklString* str=FKL_VM_STR(obj);
	size_t len=str->size;
	FklVMvalue* r=fklCreateVMvalueDvec(exe,len);
	FklVMdvec* vec=FKL_VM_DVEC(r);
	for(size_t i=0;i<len;i++)
		vec->base[i]=FKL_MAKE_VM_CHR(str->str[i]);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_dvec_ref(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,place,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_DVECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	if(index>=v->size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,v->base[index]);
	return 0;
}

static int builtin_dvec_set(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(vec,place,target,exe);
	FKL_CHECK_REST_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_DVECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(fklIsVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t index=fklGetUint(place);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t size=v->size;
	if(index>=size)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS,exe);
	v->base[index]=target;
	FKL_VM_PUSH_VALUE(exe,target);
	return 0;
}

static int builtin_dvec_fill(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(vec,content,exe)
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t size=v->size;
	for(size_t i=0;i<size;i++)
		v->base[i]=content;
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_dvec_to_string(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t size=v->size;
	FklVMvalue* r=fklCreateVMvalueStr(exe,fklCreateString(size,NULL));
	FklString* str=FKL_VM_STR(r);
	for(size_t i=0;i<size;i++)
	{
		FKL_CHECK_TYPE(v->base[i],FKL_IS_CHR,exe);
		str->str[i]=FKL_GET_CHR(v->base[i]);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_dvec_to_list(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<v->size;i++)
	{
		*cur=fklCreateVMvaluePairWithCar(exe,v->base[i]);
		cur=&FKL_VM_CDR(*cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_dvec_to_bytevector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	size_t size=v->size;
	FklVMvalue* r=fklCreateVMvalueBvec(exe,fklCreateBytevector(v->size,NULL));
	FklVMvalue** base=v->base;
	uint8_t* ptr=FKL_VM_BVEC(r)->ptr;
	for(uint64_t i=0;i<size;i++)
	{
		FklVMvalue* cur=base[i];
		FKL_CHECK_TYPE(cur,fklIsVMint,exe);
		ptr[i]=fklGetInt(cur);
	}
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int builtin_vector_to_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_VECTOR,exe);
	FklVMvec* vec=FKL_VM_VEC(obj);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueDvecWithPtr(exe,vec->size,vec->base));
	return 0;
}

static int builtin_dvec_to_vector(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(vec,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(vec,FKL_IS_DVECTOR,exe);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueVecWithPtr(exe,v->size,v->base));
	return 0;
}

static int builtin_bytevector_to_u8_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	uint8_t* u8a=bvec->ptr;
	FklVMvalue* vec=fklCreateVMvalueDvec(exe,size);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	for(size_t i=0;i<size;i++)
		v->base[i]=FKL_MAKE_VM_FIX(u8a[i]);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

static int builtin_bytevector_to_s8_dvec(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_BYTEVECTOR,exe);
	FklBytevector* bvec=FKL_VM_BVEC(obj);
	size_t size=bvec->size;
	int8_t* s8a=(int8_t*)bvec->ptr;
	FklVMvalue* vec=fklCreateVMvalueDvec(exe,size);
	FklVMdvec* v=FKL_VM_DVEC(vec);
	for(size_t i=0;i<size;i++)
		v->base[i]=FKL_MAKE_VM_FIX(s8a[i]);
	FKL_VM_PUSH_VALUE(exe,vec);
	return 0;
}

#undef PREDICATE
//end

#include<fakeLisp/opcode.h>

#define INL_FUNC_ARGS FklByteCodelnt* bcs[],FklSid_t fid,uint32_t line,uint32_t scope

static inline FklByteCodelnt* inl_0_arg_func(FklOpcode opc
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope)
{
	FklInstruction ins={.op=opc};
	return fklCreateSingleInsBclnt(ins,fid,line,scope);
}

static inline FklByteCodelnt* inlfunc_box0(INL_FUNC_ARGS)
{
	return inl_0_arg_func(FKL_OP_BOX0,fid,line,scope);
}

static inline FklByteCodelnt* inlfunc_add0(INL_FUNC_ARGS)
{
	return inl_0_arg_func(FKL_OP_PUSH_0,fid,line,scope);
}

static FklByteCodelnt* inlfunc_ret0(INL_FUNC_ARGS)
{
	FklByteCodelnt* r=inl_0_arg_func(FKL_OP_RES_BP_TP,fid,line,scope);
	FklInstruction ins={.op=FKL_OP_PUSH_NIL};
	fklByteCodeLntPushBackIns(r,&ins,fid,line,scope);
	ins.op=FKL_OP_RET;
	fklByteCodeLntPushBackIns(r,&ins,fid,line,scope);
	return r;
}

static inline FklByteCodelnt* inlfunc_mul0(INL_FUNC_ARGS)
{
	return inl_0_arg_func(FKL_OP_PUSH_1,fid,line,scope);
}

static inline FklByteCodelnt* inl_1_arg_func(FklOpcode opc
		,FklByteCodelnt* bcs[]
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope)
{
	FklInstruction bc={.op=opc};
	fklByteCodeLntPushBackIns(bcs[0],&bc,fid,line,scope);
	return bcs[0];
}

static FklByteCodelnt* inlfunc_true(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_TRUE,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_not(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_NOT,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_atom(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_ATOM,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_car(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_PUSH_CAR,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_cdr(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_PUSH_CDR,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_dvec_first(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_DVEC_FIRST,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_dvec_last(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_DVEC_LAST,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_vec_first(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_VEC_FIRST,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_vec_last(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_VEC_LAST,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_add_1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_INC,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_sub_1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_DEC,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_neg(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_NEG,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_rec(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_REC,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_add1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_ADD1,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_mul1(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_MUL1,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_box(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_BOX,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_unbox(INL_FUNC_ARGS)
{
	return inl_1_arg_func(FKL_OP_UNBOX,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_ret1(INL_FUNC_ARGS)
{
	FklByteCodelnt* r=bcs[0];
	FklInstruction ins={.op=FKL_OP_RES_BP_TP};
	fklByteCodeLntInsertFrontIns(&ins,r,fid,line,scope);
	ins.op=FKL_OP_RET;
	fklByteCodeLntPushBackIns(r,&ins,fid,line,scope);
	return r;
}

static inline FklByteCodelnt* inl_2_arg_func(FklOpcode opc
		,FklByteCodelnt* bcs[]
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope)
{
	FklInstruction bc={.op=opc};
	fklCodeLntReverseConcat(bcs[1],bcs[0]);
	fklByteCodeLntPushBackIns(bcs[0],&bc,fid,line,scope);
	fklDestroyByteCodelnt(bcs[1]);
	return bcs[0];
}

static FklByteCodelnt* inlfunc_cons(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_CONS,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_eq(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQ,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_eqv(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQV,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_equal(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQUAL,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_eqn(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_EQN,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_mul(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_MUL,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_div(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_DIV,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_idiv(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_IDIV,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_mod(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_MOD,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_nth(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_NTH,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_vec_ref(INL_FUNC_ARGS)
{
	FklByteCodelnt* index_bcl=bcs[1];
	if(index_bcl->bc->len==1&&index_bcl->bc->code[0].op==FKL_OP_PUSH_0)
	{
		fklDestroyByteCodelnt(index_bcl);
		return inl_1_arg_func(FKL_OP_VEC_FIRST,bcs,fid,line,scope);
	}
	else
		return inl_2_arg_func(FKL_OP_VEC_REF,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_dvec_ref(INL_FUNC_ARGS)
{
	FklByteCodelnt* index_bcl=bcs[1];
	if(index_bcl->bc->len==1&&index_bcl->bc->code[0].op==FKL_OP_PUSH_0)
	{
		fklDestroyByteCodelnt(index_bcl);
		return inl_1_arg_func(FKL_OP_DVEC_FIRST,bcs,fid,line,scope);
	}
	else
	return inl_2_arg_func(FKL_OP_DVEC_REF,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_str_ref(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_STR_REF,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_add(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_ADD,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_sub(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_SUB,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_gt(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_GT,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_lt(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_LT,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_ge(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_GE,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_le(INL_FUNC_ARGS)
{
	return inl_2_arg_func(FKL_OP_LE,bcs,fid,line,scope);
}

static inline FklByteCodelnt* inl_3_arg_func(FklOpcode opc
		,FklByteCodelnt* bcs[]
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope)
{
	FklInstruction bc={.op=opc};
	fklCodeLntReverseConcat(bcs[1],bcs[0]);
	fklCodeLntReverseConcat(bcs[2],bcs[0]);
	fklByteCodeLntPushBackIns(bcs[0],&bc,fid,line,scope);
	fklDestroyByteCodelnt(bcs[1]);
	fklDestroyByteCodelnt(bcs[2]);
	return bcs[0];
}

static FklByteCodelnt* inlfunc_eqn3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_EQN3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_gt3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_GT3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_lt3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_LT3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_ge3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_GE3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_le3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_LE3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_add3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_ADD3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_sub3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_SUB3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_mul3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_MUL3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_div3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_DIV3,bcs,fid,line,scope);
}

static FklByteCodelnt* inlfunc_idiv3(INL_FUNC_ARGS)
{
	return inl_3_arg_func(FKL_OP_IDIV3,bcs,fid,line,scope);
}

#undef INL_FUNC_ARGS

static const struct SymbolFuncStruct
{
	const char* s;
	FklVMcFunc f;
	FklBuiltinInlineFunc inlfunc[4];
}builtInSymbolList[FKL_BUILTIN_SYMBOL_NUM+1]=
{
	{"stdin",                 NULL,                            {NULL,         NULL,               NULL,             NULL,          }, },
	{"stdout",                NULL,                            {NULL,         NULL,               NULL,             NULL,          }, },
	{"stderr",                NULL,                            {NULL,         NULL,               NULL,             NULL,          }, },
	{"car",                   builtin_car,                     {NULL,         inlfunc_car,        NULL,             NULL,          }, },
	{"cdr",                   builtin_cdr,                     {NULL,         inlfunc_cdr,        NULL,             NULL,          }, },
	{"cons",                  builtin_cons,                    {NULL,         NULL,               inlfunc_cons,     NULL,          }, },
	{"append",                builtin_append,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"append!",               builtin_append1,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"copy",                  builtin_copy,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"atom",                  builtin_atom,                    {NULL,         inlfunc_atom,       NULL,             NULL,          }, },
	{"null",                  builtin_null,                    {NULL,         inlfunc_not,        NULL,             NULL,          }, },
	{"not",                   builtin_not,                     {NULL,         inlfunc_not,        NULL,             NULL,          }, },
	{"eq",                    builtin_eq,                      {NULL,         NULL,               inlfunc_eq,       NULL,          }, },
	{"eqv",                   builtin_eqv,                     {NULL,         NULL,               inlfunc_eqv,      NULL,          }, },
	{"equal",                 builtin_equal,                   {NULL,         NULL,               inlfunc_equal,    NULL,          }, },
	{"=",                     builtin_eqn,                     {NULL,         inlfunc_true,       inlfunc_eqn,      inlfunc_eqn3,  }, },
	{"+",                     builtin_add,                     {inlfunc_add0, inlfunc_add1,       inlfunc_add,      inlfunc_add3,  }, },
	{"1+",                    builtin_add_1,                   {NULL,         inlfunc_add_1,      NULL,             NULL,          }, },
	{"-",                     builtin_sub,                     {NULL,         inlfunc_neg,        inlfunc_sub,      inlfunc_sub3,  }, },
	{"-1+",                   builtin_sub_1,                   {NULL,         inlfunc_sub_1,      NULL,             NULL,          }, },
	{"*",                     builtin_mul,                     {inlfunc_mul0, inlfunc_mul1,       inlfunc_mul,      inlfunc_mul3,  }, },
	{"/",                     builtin_div,                     {NULL,         inlfunc_rec,        inlfunc_div,      inlfunc_div3,  }, },
	{"//",                    builtin_idiv,                    {NULL,         NULL,               inlfunc_idiv,     inlfunc_idiv3, }, },
	{"%",                     builtin_mod,                     {NULL,         NULL,               inlfunc_mod,      NULL,          }, },
	{">",                     builtin_gt,                      {NULL,         inlfunc_true,       inlfunc_gt,       inlfunc_gt3,   }, },
	{">=",                    builtin_ge,                      {NULL,         inlfunc_true,       inlfunc_ge,       inlfunc_ge3,   }, },
	{"<",                     builtin_lt,                      {NULL,         inlfunc_true,       inlfunc_lt,       inlfunc_lt3,   }, },
	{"<=",                    builtin_le,                      {NULL,         inlfunc_true,       inlfunc_le,       inlfunc_le3,   }, },
	{"nth",                   builtin_nth,                     {NULL,         NULL,               inlfunc_nth,      NULL,          }, },
	{"length",                builtin_length,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"apply",                 builtin_apply,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"call/eh",               builtin_call_eh,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"read",                  builtin_read,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"parse",                 builtin_parse,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-parser",           builtin_make_parser,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"parser?",               builtin_parser_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"stringify",             builtin_stringify,               {NULL,         NULL,               NULL,             NULL,          }, },

	{"prin1",                 builtin_prin1,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"princ",                 builtin_princ,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"println",               builtin_println,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"print",                 builtin_print,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"printf",                builtin_printf,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"format",                builtin_format,                  {NULL,         NULL,               NULL,             NULL,          }, },

	{"prin1n",                builtin_prin1n,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"prin1v",                builtin_prin1v,                  {NULL,         NULL,               NULL,             NULL,          }, },

	{"newline",               builtin_newline,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"dlopen",                builtin_dlopen,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"dlsym",                 builtin_dlsym,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"argv",                  builtin_argv,                    {NULL,         NULL,               NULL,             NULL,          }, },

	{"atexit",                builtin_atexit,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"idle",                  builtin_idle,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"go",                    builtin_go,                      {NULL,         NULL,               NULL,             NULL,          }, },
	{"pcall",                 builtin_pcall,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"chanl",                 builtin_chanl,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"chanl-msg-num",         builtin_chanl_msg_num,           {NULL,         NULL,               NULL,             NULL,          }, },
	{"chanl-recv-num",        builtin_chanl_recv_num,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"chanl-send-num",        builtin_chanl_send_num,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"chanl-full?",           builtin_chanl_full_p,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"chanl-empty?",          builtin_chanl_empty_p,           {NULL,         NULL,               NULL,             NULL,          }, },
	{"chanl-msg->list",       builtin_chanl_msg_to_list,       {NULL,         NULL,               NULL,             NULL,          }, },
	{"send",                  builtin_send,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"recv",                  builtin_recv,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"recv&",                 builtin_recv7,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"error",                 builtin_error,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"error-type",            builtin_error_type,              {NULL,         NULL,               NULL,             NULL,          }, },
	{"error-msg",             builtin_error_msg,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"raise",                 builtin_raise,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"throw",                 builtin_throw,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"reverse",               builtin_reverse,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"reverse!",              builtin_reverse1,                {NULL,         NULL,               NULL,             NULL,          }, },

	{"nthcdr",                builtin_nthcdr,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"tail",                  builtin_tail,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"char?",                 builtin_char_p,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"integer?",              builtin_integer_p,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"fixint?",               builtin_fixint_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bigint?",               builtin_bigint_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"f64?",                  builtin_f64_p,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"pair?",                 builtin_pair_p,                  {NULL,         NULL,               NULL,             NULL,          }, },

	{"symbol?",               builtin_symbol_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"string->symbol",        builtin_string_to_symbol,        {NULL,         NULL,               NULL,             NULL,          }, },

	{"string?",               builtin_string_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"string",                builtin_string,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"substring",             builtin_substring,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"sub-string",            builtin_sub_string,              {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-string",           builtin_make_string,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"symbol->string",        builtin_symbol_to_string,        {NULL,         NULL,               NULL,             NULL,          }, },
	{"number->string",        builtin_number_to_string,        {NULL,         NULL,               NULL,             NULL,          }, },
	{"integer->string",       builtin_integer_to_string,       {NULL,         NULL,               NULL,             NULL,          }, },
	{"f64->string",           builtin_f64_to_string,           {NULL,         NULL,               NULL,             NULL,          }, },
	{"vector->string",        builtin_vector_to_string,        {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->string",    builtin_bytevector_to_string,    {NULL,         NULL,               NULL,             NULL,          }, },
	{"list->string",          builtin_list_to_string,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"->string",              builtin_to_string,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"str-ref",               builtin_str_ref,                 {NULL,         NULL,               inlfunc_str_ref,  NULL,          }, },
	{"str-set!",              builtin_str_set1,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"string-fill!",          builtin_string_fill,             {NULL,         NULL,               NULL,             NULL,          }, },

	{"error?",                builtin_error_p,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"procedure?",            builtin_procedure_p,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"proc?",                 builtin_proc_p,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"cproc?",                builtin_cproc_p,                 {NULL,         NULL,               NULL,             NULL,          }, },

	{"vector?",               builtin_vector_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"vector",                builtin_vector,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-vector",           builtin_make_vector,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"subvector",             builtin_subvector,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"sub-vector",            builtin_sub_vector,              {NULL,         NULL,               NULL,             NULL,          }, },
	{"list->vector",          builtin_list_to_vector,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"string->vector",        builtin_string_to_vector,        {NULL,         NULL,               NULL,             NULL,          }, },
	{"vec-ref",               builtin_vec_ref,                 {NULL,         NULL,               inlfunc_vec_ref,  NULL,          }, },
	{"vec-set!",              builtin_vec_set,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"vec-cas!",              builtin_vec_cas,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"vector-fill!",          builtin_vector_fill,             {NULL,         NULL,               NULL,             NULL,          }, },

	{"list?",                 builtin_list_p,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"list",                  builtin_list,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"list*",                 builtin_list8,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-list",             builtin_make_list,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"vector->list",          builtin_vector_to_list,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"string->list",          builtin_string_to_list,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"nth-set!",              builtin_nth_set,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"nthcdr-set!",           builtin_nthcdr_set,              {NULL,         NULL,               NULL,             NULL,          }, },

	{"bytevector?",           builtin_bytevector_p,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector",            builtin_bytevector,              {NULL,         NULL,               NULL,             NULL,          }, },
	{"subbytevector",         builtin_subbytevector,           {NULL,         NULL,               NULL,             NULL,          }, },
	{"sub-bytevector",        builtin_sub_bytevector,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-bytevector",       builtin_make_bytevector,         {NULL,         NULL,               NULL,             NULL,          }, },
	{"string->bytevector",    builtin_string_to_bytevector,    {NULL,         NULL,               NULL,             NULL,          }, },
	{"vector->bytevector",    builtin_vector_to_bytevector,    {NULL,         NULL,               NULL,             NULL,          }, },
	{"list->bytevector",      builtin_list_to_bytevector,      {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->s8-list",   builtin_bytevector_to_s8_list,   {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->u8-list",   builtin_bytevector_to_u8_list,   {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->s8-vector", builtin_bytevector_to_s8_vector, {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->u8-vector", builtin_bytevector_to_u8_vector, {NULL,         NULL,               NULL,             NULL,          }, },

	{"bvs8ref",               builtin_bvs8ref,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvs16ref",              builtin_bvs16ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvs32ref",              builtin_bvs32ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvs64ref",              builtin_bvs64ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu8ref",               builtin_bvu8ref,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu16ref",              builtin_bvu16ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu32ref",              builtin_bvu32ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu64ref",              builtin_bvu64ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvf32ref",              builtin_bvf32ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvf64ref",              builtin_bvf64ref,                {NULL,         NULL,               NULL,             NULL,          }, },

	{"bvs8set!",              builtin_bvs8set1,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvs16set!",             builtin_bvs16set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvs32set!",             builtin_bvs32set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvs64set!",             builtin_bvs64set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu8set!",              builtin_bvu8set1,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu16set!",             builtin_bvu16set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu32set!",             builtin_bvu32set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvu64set!",             builtin_bvu64set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvf32set!",             builtin_bvf32set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bvf64set!",             builtin_bvf64set1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector-fill!",      builtin_bytevector_fill,         {NULL,         NULL,               NULL,             NULL,          }, },

	{"chanl?",                builtin_chanl_p,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"dll?",                  builtin_dll_p,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"fgets",                 builtin_fgets,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"fgetb",                 builtin_fgetb,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"fgetd",                 builtin_fgetd,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"fgetc",                 builtin_fgetc,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"fgeti",                 builtin_fgeti,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"fopen",                 builtin_fopen,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"fclose",                builtin_fclose,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"feof?",                 builtin_feof_p,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"eof?",                  builtin_eof_p,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"ftell",                 builtin_ftell,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"fseek",                 builtin_fseek,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"car-set!",              builtin_car_set,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"cdr-set!",              builtin_cdr_set,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"box",                   builtin_box,                     {inlfunc_box0, inlfunc_box,        NULL,             NULL,          }, },
	{"unbox",                 builtin_unbox,                   {NULL,         inlfunc_unbox,      NULL,             NULL,          }, },
	{"box-set!",              builtin_box_set,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"box-cas!",              builtin_box_cas,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"box?",                  builtin_box_p,                   {NULL,         NULL,               NULL,             NULL,          }, },

	{"number?",               builtin_number_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"string->number",        builtin_string_to_number,        {NULL,         NULL,               NULL,             NULL,          }, },
	{"char->integer",         builtin_char_to_integer,         {NULL,         NULL,               NULL,             NULL,          }, },
	{"symbol->integer",       builtin_symbol_to_integer,       {NULL,         NULL,               NULL,             NULL,          }, },
	{"integer->char",         builtin_integer_to_char,         {NULL,         NULL,               NULL,             NULL,          }, },
	{"number->f64",           builtin_number_to_f64,           {NULL,         NULL,               NULL,             NULL,          }, },
	{"number->integer",       builtin_number_to_integer,       {NULL,         NULL,               NULL,             NULL,          }, },

	{"map",                   builtin_map,                     {NULL,         NULL,               NULL,             NULL,          }, },
	{"foreach",               builtin_foreach,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"andmap",                builtin_andmap,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"ormap",                 builtin_ormap,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"memq",                  builtin_memq,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"member",                builtin_member,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"memp",                  builtin_memp,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"filter",                builtin_filter,                  {NULL,         NULL,               NULL,             NULL,          }, },

	{"remq!",                 builtin_remq1,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"remv!",                 builtin_remv1,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"remove!",               builtin_remove1,                 {NULL,         NULL,               NULL,             NULL,          }, },

	{"sleep",                 builtin_sleep,                   {NULL,         NULL,               NULL,             NULL,          }, },
	{"msleep",                builtin_msleep,                  {NULL,         NULL,               NULL,             NULL,          }, },

	{"hash",                  builtin_hash,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-num",              builtin_hash_num,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-hash",             builtin_make_hash,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hasheqv",               builtin_hasheqv,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-hasheqv",          builtin_make_hasheqv,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"hashequal",             builtin_hashequal,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-hashequal",        builtin_make_hashequal,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash?",                 builtin_hash_p,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"hasheq?",               builtin_hasheq_p,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"hasheqv?",              builtin_hasheqv_p,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hashequal?",            builtin_hashequal_p,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-ref",              builtin_hash_ref,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-ref&",             builtin_hash_ref7,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-ref$",             builtin_hash_ref4,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-ref!",             builtin_hash_ref1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-set!",             builtin_hash_set,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-set*!",            builtin_hash_set8,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-del!",             builtin_hash_del1,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-clear!",           builtin_hash_clear,              {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash->list",            builtin_hash_to_list,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-keys",             builtin_hash_keys,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"hash-values",           builtin_hash_values,             {NULL,         NULL,               NULL,             NULL,          }, },

	{"pmatch",                builtin_pmatch,                  {NULL,         NULL,               NULL,             NULL,          }, },

	{"exit",                  builtin_exit,                    {NULL,         NULL,               NULL,             NULL,          }, },

	{"return",                builtin_return,                  {inlfunc_ret0, inlfunc_ret1,       NULL,             NULL,          }, },

	{"dvec?",                 builtin_dvec_p,                  {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec",                  builtin_dvec,                    {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-dvec",             builtin_make_dvec,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"make-dvec/capacity",    builtin_make_dvec_with_capacity, {NULL,         NULL,               NULL,             NULL,          }, },
	{"subdvec",               builtin_subdvec,                 {NULL,         NULL,               NULL,             NULL,          }, },
	{"sub-dvec",              builtin_sub_dvec,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-empty?",           builtin_dvec_empty_p,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-capacity",         builtin_dvec_capacity,           {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-first",            builtin_dvec_first,              {NULL,         inlfunc_dvec_first, NULL,             NULL,          }, },
	{"dvec-last",             builtin_dvec_last,               {NULL,         inlfunc_dvec_last,  NULL,             NULL,          }, },
	{"dvec-ref",              builtin_dvec_ref,                {NULL,         NULL,               inlfunc_dvec_ref, NULL,          }, },
	{"dvec-set!",             builtin_dvec_set,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-assign!",          builtin_dvec_assign,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-fill!",            builtin_dvec_fill,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-clear!",           builtin_dvec_clear,              {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-reserve!",         builtin_dvec_reserve,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-push!",            builtin_dvec_push,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-pop!",             builtin_dvec_pop,                {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-pop&!",            builtin_dvec_pop7,               {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-remove!",          builtin_dvec_remove,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-insert!",          builtin_dvec_insert,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-shrink!",          builtin_dvec_shrink,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec-resize!",          builtin_dvec_resize,             {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec->string",          builtin_dvec_to_string,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec->list",            builtin_dvec_to_list,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec->vector",          builtin_dvec_to_vector,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"dvec->bytevector",      builtin_dvec_to_bytevector,      {NULL,         NULL,               NULL,             NULL,          }, },
	{"vector->dvec",          builtin_vector_to_dvec,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"list->dvec",            builtin_list_to_dvec,            {NULL,         NULL,               NULL,             NULL,          }, },
	{"string->dvec",          builtin_string_to_dvec,          {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->s8-dvec",   builtin_bytevector_to_s8_dvec,   {NULL,         NULL,               NULL,             NULL,          }, },
	{"bytevector->u8-dvec",   builtin_bytevector_to_u8_dvec,   {NULL,         NULL,               NULL,             NULL,          }, },
	{"vec-first",             builtin_vec_first,               {NULL,         inlfunc_vec_first,  NULL,             NULL,          }, },
	{"vec-last",              builtin_vec_last,                {NULL,         inlfunc_vec_last,   NULL,             NULL,          }, },
	{NULL,                    NULL,                            {NULL,         NULL,               NULL,             NULL,          }, },
};

FklBuiltinInlineFunc fklGetBuiltinInlineFunc(uint32_t idx,uint32_t argNum)
{
	if(idx>=FKL_BUILTIN_SYMBOL_NUM)
		return NULL;
	return builtInSymbolList[idx].inlfunc[argNum];
}

void fklInitGlobCodegenEnv(FklCodegenEnv* curEnv,FklSymbolTable* pst)
{
	for(const struct SymbolFuncStruct* list=builtInSymbolList
			;list->s!=NULL
			;list++)
	{
		FklSymbolDef* ref=fklAddCodegenBuiltinRefBySid(fklAddSymbolCstr(list->s,pst)->id,curEnv);
		ref->isConst=1;
		// FklSymbolDef* def=fklAddCodegenDefBySid(fklAddSymbolCstr(list->s,pst)->id,1,curEnv);
	}
}

static inline void init_vm_public_data(PublicBuiltInData* pd,FklVM* exe)
{
	FklVMvalue* builtInStdin=fklCreateVMvalueFp(exe,stdin,FKL_VM_FP_R);
	FklVMvalue* builtInStdout=fklCreateVMvalueFp(exe,stdout,FKL_VM_FP_W);
	FklVMvalue* builtInStderr=fklCreateVMvalueFp(exe,stderr,FKL_VM_FP_W);
	pd->sysIn=builtInStdin;
	pd->sysOut=builtInStdout;
	pd->sysErr=builtInStderr;

	fklVMacquireSt(exe->gc);
	FklSymbolTable* st=exe->gc->st;
	pd->seek_set=fklAddSymbolCstr("set",st)->id;
	pd->seek_cur=fklAddSymbolCstr("cur",st)->id;
	pd->seek_end=fklAddSymbolCstr("end",st)->id;
	fklVMreleaseSt(exe->gc);
}

void fklInitGlobalVMclosureForGC(FklVM* exe)
{
	FklVMgc* gc=exe->gc;
	FklVMvalue** closure=gc->builtin_refs;
	FklVMvalue* publicUserData=fklCreateVMvalueUd(exe
			,&PublicBuiltInDataMetaTable
			,FKL_VM_NIL);

	FKL_DECL_VM_UD_DATA(pd,PublicBuiltInData,publicUserData);
	init_vm_public_data(pd,exe);

	closure[FKL_VM_STDIN_IDX]=fklCreateClosedVMvalueVarRef(exe,pd->sysIn);
	closure[FKL_VM_STDOUT_IDX]=fklCreateClosedVMvalueVarRef(exe,pd->sysOut);
	closure[FKL_VM_STDERR_IDX]=fklCreateClosedVMvalueVarRef(exe,pd->sysErr);

	fklVMacquireSt(exe->gc);
	FklSymbolTable* st=exe->gc->st;
	for(size_t i=3;i<FKL_BUILTIN_SYMBOL_NUM;i++)
	{
		FklVMvalue* v=fklCreateVMvalueCproc(exe
				,builtInSymbolList[i].f
				,NULL
				,publicUserData
				,fklAddSymbolCstr(builtInSymbolList[i].s,st)->id);
		closure[i]=fklCreateClosedVMvalueVarRef(exe,v);
	}
	fklVMreleaseSt(exe->gc);
}

