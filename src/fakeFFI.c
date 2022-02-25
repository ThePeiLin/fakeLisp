#include<fakeLisp/fakeFFI.h>
#include<fakeLisp/common.h>
#include<fakeLisp/VMtool.h>
#include<stddef.h>
#include<pthread.h>
extern FklTypeId_t LastNativeTypeId;
static pthread_mutex_t GPrepCifLock=PTHREAD_MUTEX_INITIALIZER;
static ffi_type* NativeFFITypeList[]=
{
	&ffi_type_void,
	&ffi_type_sshort,
	&ffi_type_sint,
	&ffi_type_ushort,
	&ffi_type_uint,
	&ffi_type_slong,
	&ffi_type_ulong,
	&ffi_type_slong,//long long
	&ffi_type_ulong,//unsigned long long
	&ffi_type_slong,//ptrdiff_t
	&ffi_type_ulong,//size_t
	&ffi_type_slong,//ssize_t
	&ffi_type_schar,//char
	&ffi_type_sint32,//wchar_t
	&ffi_type_float,
	&ffi_type_double,
	&ffi_type_sint8,
	&ffi_type_uint8,
	&ffi_type_sint16,
	&ffi_type_uint16,
	&ffi_type_sint32,
	&ffi_type_uint32,
	&ffi_type_sint64,
	&ffi_type_uint64,
	&ffi_type_slong,//intptr_t
	&ffi_type_ulong,//uintptr_t
	&ffi_type_pointer,//void*
};

/*cast all mem to int64*/
#define ARGL uint8_t* mem
#define CAST(TYPE) return (int64_t)*(TYPE*)mem;
static int64_t castToInt64WithShort    (ARGL){CAST(short)}
static int64_t castToInt64WithUShort   (ARGL){CAST(unsigned short)}
static int64_t castToInt64WithInt      (ARGL){CAST(int)}
static int64_t castToInt64WithUint     (ARGL){CAST(unsigned int)}
static int64_t castToInt64WithLong     (ARGL){CAST(long)}
static int64_t castToInt64WithULong    (ARGL){CAST(unsigned long)}
static int64_t castToInt64WithLLong    (ARGL){CAST(long long)}
static int64_t castToInt64WithULLong   (ARGL){CAST(unsigned long long)}
static int64_t castToInt64WithPtrdiff_t(ARGL){CAST(ptrdiff_t)}
static int64_t castToInt64WithSize_t   (ARGL){CAST(size_t)}
static int64_t castToInt64WithSsize_t  (ARGL){CAST(ssize_t)}
static int64_t castToInt64WithChar     (ARGL){CAST(char)}
static int64_t castToInt64WithWchar_t  (ARGL){CAST(wchar_t)}
static int64_t castToInt64WithFloat    (ARGL){CAST(float)}
static int64_t castToInt64WithDouble   (ARGL){CAST(double)}
static int64_t castToInt64WithInt8_t   (ARGL){CAST(int8_t)}
static int64_t castToInt64WithUint8_t  (ARGL){CAST(uint8_t)}
static int64_t castToInt64WithInt16_t  (ARGL){CAST(int16_t)}
static int64_t castToInt64WithUint16_t (ARGL){CAST(uint16_t)}
static int64_t castToInt64WithInt32_t  (ARGL){CAST(int32_t)}
static int64_t castToInt64WithUint32_t (ARGL){CAST(int32_t)}
static int64_t castToInt64WithInt64_t  (ARGL){CAST(int64_t)}
static int64_t castToInt64WithUint64_t (ARGL){CAST(uint64_t)}
static int64_t castToInt64WithIptr     (ARGL){CAST(intptr_t)}
static int64_t castToInt64WithUptr     (ARGL){CAST(uintptr_t)}
static int64_t castToInt64WithVptr     (ARGL){return (int64_t)mem;}

#undef CAST
static int64_t (*castToInt64FunctionsList[])(ARGL)=
{
	castToInt64WithShort    ,
	castToInt64WithUShort   ,
	castToInt64WithInt      ,
	castToInt64WithUint     ,
	castToInt64WithLong     ,
	castToInt64WithULong    ,
	castToInt64WithLLong    ,
	castToInt64WithULLong   ,
	castToInt64WithPtrdiff_t,
	castToInt64WithSize_t   ,
	castToInt64WithSsize_t  ,
	castToInt64WithChar     ,
	castToInt64WithWchar_t  ,
	castToInt64WithFloat    ,
	castToInt64WithDouble   ,
	castToInt64WithInt8_t   ,
	castToInt64WithUint8_t  ,
	castToInt64WithInt16_t  ,
	castToInt64WithUint16_t ,
	castToInt64WithInt32_t  ,
	castToInt64WithUint32_t ,
	castToInt64WithInt64_t  ,
	castToInt64WithUint64_t ,
	castToInt64WithIptr     ,
	castToInt64WithUptr     ,
	castToInt64WithVptr     ,
};
#undef ARGL
/*---------------------*/

/*cast all mem to double*/
#define ARGL uint8_t* mem
#define CAST(TYPE) return (double)*(TYPE*)mem;
static double castToDoubleWithShort    (ARGL){CAST(short)}
static double castToDoubleWithUShort   (ARGL){CAST(unsigned short)}
static double castToDoubleWithInt      (ARGL){CAST(int)}
static double castToDoubleWithUint     (ARGL){CAST(unsigned int)}
static double castToDoubleWithLong     (ARGL){CAST(long)}
static double castToDoubleWithULong    (ARGL){CAST(unsigned long)}
static double castToDoubleWithLLong    (ARGL){CAST(long long)}
static double castToDoubleWithULLong   (ARGL){CAST(unsigned long long)}
static double castToDoubleWithPtrdiff_t(ARGL){CAST(ptrdiff_t)}
static double castToDoubleWithSize_t   (ARGL){CAST(size_t)}
static double castToDoubleWithSsize_t  (ARGL){CAST(ssize_t)}
static double castToDoubleWithChar     (ARGL){CAST(char)}
static double castToDoubleWithWchar_t  (ARGL){CAST(wchar_t)}
static double castToDoubleWithFloat    (ARGL){CAST(float)}
static double castToDoubleWithDouble   (ARGL){CAST(double)}
static double castToDoubleWithInt8_t   (ARGL){CAST(int8_t)}
static double castToDoubleWithUint8_t  (ARGL){CAST(uint8_t)}
static double castToDoubleWithInt16_t  (ARGL){CAST(int16_t)}
static double castToDoubleWithUint16_t (ARGL){CAST(uint16_t)}
static double castToDoubleWithInt32_t  (ARGL){CAST(int32_t)}
static double castToDoubleWithUint32_t (ARGL){CAST(int32_t)}
static double castToDoubleWithInt64_t  (ARGL){CAST(int64_t)}
static double castToDoubleWithUint64_t (ARGL){CAST(uint64_t)}
static double castToDoubleWithIptr     (ARGL){CAST(intptr_t)}
static double castToDoubleWithUptr     (ARGL){CAST(uintptr_t)}
static double castToDoubleWithVptr     (ARGL){return (int64_t)mem;}

#undef CAST
static double (*castToDoubleFunctionsList[])(ARGL)=
{
	castToDoubleWithShort    ,
	castToDoubleWithUShort   ,
	castToDoubleWithInt      ,
	castToDoubleWithUint     ,
	castToDoubleWithLong     ,
	castToDoubleWithULong    ,
	castToDoubleWithLLong    ,
	castToDoubleWithULLong   ,
	castToDoubleWithPtrdiff_t,
	castToDoubleWithSize_t   ,
	castToDoubleWithSsize_t  ,
	castToDoubleWithChar     ,
	castToDoubleWithWchar_t  ,
	castToDoubleWithFloat    ,
	castToDoubleWithDouble   ,
	castToDoubleWithInt8_t   ,
	castToDoubleWithUint8_t  ,
	castToDoubleWithInt16_t  ,
	castToDoubleWithUint16_t ,
	castToDoubleWithInt32_t  ,
	castToDoubleWithUint32_t ,
	castToDoubleWithInt64_t  ,
	castToDoubleWithUint64_t ,
	castToDoubleWithIptr     ,
	castToDoubleWithUptr     ,
	castToDoubleWithVptr     ,
};
#undef ARGL
/*----------------------*/

/*ffi_type pointer caster functions*/
#define ARGL FklVMvalue* v,void** p
#define CAST_TO_INT(TYPE) TYPE* t=(TYPE*)malloc(sizeof(TYPE));\
	FAKE_ASSERT(t,"VMvalue_pointer_caster",__FILE__,__LINE__);\
	if(!IS_MEM(v)&&!IS_CHF(v))\
	{\
		FklVMptrTag tag=GET_TAG(v);\
		switch(tag)\
		{\
			case NIL_TAG:\
				*t=(TYPE)0x0;\
				break;\
			case FKL_I32_TAG:\
				*t=(TYPE)GET_I32(v);\
					break;\
			case FKL_CHR_TAG:\
				*t=(TYPE)GET_CHR(v);\
				break;\
			case FKL_SYM_TAG:\
				*t=(TYPE)GET_SYM(v);\
				break;\
			case PTR_TAG:\
				{\
					switch(v->type)\
					{\
						case FKL_DBL:\
							*t=(TYPE)*v->u.dbl;\
							break;\
						case FKL_I64:\
							*t=(TYPE)*v->u.i64;\
							break;\
						default:\
							return 1;\
							break;\
					}\
				}\
			default:\
				return 1;\
				break;\
		}\
	}\
	else\
		*t=castToInt64FunctionsList[v->u.chf->type-1](v->u.chf->mem);\
	*p=t;\
	return 0;

#define CAST_TO_FLOAT(TYPE) if(!IS_I32(v)&&!IS_I64(v)&&!IS_CHR(v)&&!IS_DBL(v)&&!IS_MEM(v)&&!IS_CHF(v))return 1;\
	TYPE* t=(TYPE*)malloc(sizeof(TYPE));\
	FAKE_ASSERT(t,"VMvalue_pointer_caster",__FILE__,__LINE__);\
	if(!IS_MEM(v)&&!IS_CHF(v))\
		*t=IS_I32(v)?GET_I32(v):(IS_I64(v)?*v->u.i64:(IS_DBL(v)?*v->u.dbl:GET_CHR(v)));\
	else\
		*t=castToDoubleFunctionsList[v->u.chf->type-1](v->u.chf->mem);\
	*p=t;\
	return 0;

static int castShortValue    (ARGL){CAST_TO_INT(short)}
static int castIntValue      (ARGL){CAST_TO_INT(int)}
static int castUShortValue   (ARGL){CAST_TO_INT(unsigned short)}
static int castUintValue     (ARGL){CAST_TO_INT(unsigned int)}
static int castLongValue     (ARGL){CAST_TO_INT(long)}
static int castULongValue    (ARGL){CAST_TO_INT(unsigned long)}
static int castLLongValue    (ARGL){CAST_TO_INT(long long)}
static int castULLongValue   (ARGL){CAST_TO_INT(unsigned long long)}
static int castPtrdiff_tValue(ARGL){CAST_TO_INT(ptrdiff_t)}
static int castSize_tValue   (ARGL){CAST_TO_INT(size_t)}
static int castSsize_tValue  (ARGL){CAST_TO_INT(ssize_t)}
static int castCharValue     (ARGL){CAST_TO_INT(char)}
static int castWchar_tValue  (ARGL){CAST_TO_INT(wchar_t)}
static int castFloatValue    (ARGL){CAST_TO_FLOAT(float)}
static int castDoubleValue   (ARGL){CAST_TO_FLOAT(double)}
static int castInt8_tValue   (ARGL){CAST_TO_INT(int8_t)}
static int castUint8_tValue  (ARGL){CAST_TO_INT(uint8_t)}
static int castInt16_tValue  (ARGL){CAST_TO_INT(int16_t)}
static int castUint16_tValue (ARGL){CAST_TO_INT(uint16_t)}
static int castInt32_tValue  (ARGL){CAST_TO_INT(int32_t)}
static int castUint32_tValue (ARGL){CAST_TO_INT(uint32_t)}
static int castInt64_tValue  (ARGL){CAST_TO_INT(int64_t)}
static int castUint64_tValue (ARGL){CAST_TO_INT(uint64_t)}
static int castIptrValue     (ARGL){CAST_TO_INT(intptr_t)}
static int castUptrValue     (ARGL){CAST_TO_INT(uintptr_t)}
static int castVptrValue     (ARGL)
{
	void** t=(void**)malloc(sizeof(void*));
	FAKE_ASSERT(t,"VMvalue_pointer_caster",__FILE__,__LINE__);
	if(!IS_MEM(v)&&!IS_CHF(v))
	{
		if(v==VM_NIL)
			*t=NULL;
		else if(IS_PTR(v))
		{
			switch(v->type)
			{
				case FKL_STR:
					*t=v->u.str;
					break;
				case FKL_BYTS:
					*t=v->u.byts->str;
					break;
				case FKL_FP:
					*t=v->u.fp;
					break;
				default:
					return 1;
					break;
			}
		}
		else
			return 1;
	}
	else
		*t=v->u.chf->mem;
	*p=t;
	return 0;
}

#undef CAST_TO_FLOAT
#undef CAST_TO_INT
static int (*fklCastValueToVptrFunctionsList[])(ARGL)=
{
	castShortValue    ,
	castIntValue      ,
	castUShortValue   ,
	castUintValue     ,
	castLongValue     ,
	castULongValue    ,
	castLLongValue    ,
	castULLongValue   ,
	castPtrdiff_tValue,
	castSize_tValue   ,
	castSsize_tValue  ,
	castCharValue     ,
	castWchar_tValue  ,
	castFloatValue    ,
	castDoubleValue   ,
	castInt8_tValue   ,
	castUint8_tValue  ,
	castInt16_tValue  ,
	castUint16_tValue ,
	castInt32_tValue  ,
	castUint32_tValue ,
	castInt64_tValue  ,
	castUint64_tValue ,
	castIptrValue     ,
	castUptrValue     ,
	castVptrValue     ,
};

#undef ARGL
/*---------------------------------*/

void fklPrepFFIcif(ffi_cif* cif,int argc,ffi_type** atypes,ffi_type* rtype)
{
	pthread_mutex_lock(&GPrepCifLock);
	ffi_status r=ffi_prep_cif(cif,FFI_DEFAULT_ABI,argc,rtype,atypes);
	pthread_mutex_unlock(&GPrepCifLock);
	FAKE_ASSERT(r==FFI_OK,"fklPrepFFIcif",__FILE__,__LINE__);
}

void fklApplyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue)
{
	ffi_cif cif;
	pthread_mutex_lock(&GPrepCifLock);
	int r=ffi_prep_cif(&cif,FFI_DEFAULT_ABI,argc,rtype,atypes);
	pthread_mutex_unlock(&GPrepCifLock);
	if(r==FFI_OK)
		ffi_call(&cif, func, rvalue, avalue);
}

void fklApplyFlproc(FklVMFlproc* f,void* rvalue,void** avalue)
{
	ffi_call(&f->cif,f->func,rvalue,avalue);
}

ffi_type* fklGetFfiType(FklTypeId_t type)
{
	if(type>LastNativeTypeId)
		type=LastNativeTypeId;
	return NativeFFITypeList[type];
}

int fklCastValueToVptr(FklTypeId_t type,FklVMvalue* v,void** p)
{
	if(fklIsFunctionTypeId(type))
	{
		if(v->u.flproc->type!=type)
			return 1;
		else
		{
			void** t=(void*)malloc(sizeof(void*));
			FAKE_ASSERT(p,"fklCastValueToVptr",__FILE__,__LINE__);
			*t=v->u.flproc->func;
			*p=t;
		}
		return 0;
	}
	else if(type==0||type>LastNativeTypeId)
		type=LastNativeTypeId;
	return fklCastValueToVptrFunctionsList[type-1](v,p);
}
