#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<stddef.h>
#include<pthread.h>

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
	FKL_ASSERT(t,__func__);\
	if(!FKL_IS_MEM(v)&&!FKL_IS_CHF(v))\
	{\
		FklVMptrTag tag=FKL_GET_TAG(v);\
		switch(tag)\
		{\
			case FKL_NIL_TAG:\
				*t=(TYPE)0x0;\
				break;\
			case FKL_I32_TAG:\
				*t=(TYPE)FKL_GET_I32(v);\
					break;\
			case FKL_CHR_TAG:\
				*t=(TYPE)FKL_GET_CHR(v);\
				break;\
			case FKL_SYM_TAG:\
				*t=(TYPE)FKL_GET_SYM(v);\
				break;\
			case FKL_PTR_TAG:\
				{\
					switch(v->type)\
					{\
						case FKL_F64:\
							*t=(TYPE)v->u.f64;\
							break;\
						case FKL_I64:\
							*t=(TYPE)v->u.i64;\
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

#define CAST_TO_FLOAT(TYPE) if(!FKL_IS_I32(v)&&!FKL_IS_I64(v)&&!FKL_IS_CHR(v)&&!FKL_IS_F64(v)&&!FKL_IS_MEM(v)&&!FKL_IS_CHF(v))return 1;\
	TYPE* t=(TYPE*)malloc(sizeof(TYPE));\
	FKL_ASSERT(t,__func__);\
	if(!FKL_IS_MEM(v)&&!FKL_IS_CHF(v))\
		*t=FKL_IS_I32(v)?FKL_GET_I32(v):(FKL_IS_I64(v)?v->u.i64:(FKL_IS_F64(v)?v->u.f64:FKL_GET_CHR(v)));\
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
	FKL_ASSERT(t,__func__);
	if(!FKL_IS_MEM(v)&&!FKL_IS_CHF(v))
	{
		if(v==FKL_VM_NIL)
			*t=NULL;
		else if(FKL_IS_PTR(v))
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
					*t=v->u.fp->fp;
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

/*print mem ref func list*/
#define PRINT_MEM_REF(FMT,TYPE,MEM,FP) fprintf(FP,FMT,*(TYPE*)(MEM))
#define ARGL uint8_t* mem,FILE* fp
static void printShortMem (ARGL){PRINT_MEM_REF("%d",short,mem,fp);}
static void printIntMem   (ARGL){PRINT_MEM_REF("%d",int,mem,fp);}
static void printUShortMem(ARGL){PRINT_MEM_REF("%u",unsigned short,mem,fp);}
static void printUIntMem  (ARGL){PRINT_MEM_REF("%u",unsigned int,mem,fp);}
static void printLongMem  (ARGL){PRINT_MEM_REF("%ld",long,mem,fp);}
static void printULongMem (ARGL){PRINT_MEM_REF("%lu",unsigned long,mem,fp);}
static void printLLongMem (ARGL){PRINT_MEM_REF("%lld",long long,mem,fp);}
static void printULLongMem(ARGL){PRINT_MEM_REF("%llu",unsigned long long,mem,fp);}
static void printPtrdiff_t(ARGL){PRINT_MEM_REF("%ld",ptrdiff_t,mem,fp);}
static void printSize_t   (ARGL){PRINT_MEM_REF("%zu",size_t,mem,fp);};
static void printSsize_t  (ARGL){PRINT_MEM_REF("%zd",ssize_t,mem,fp);}
static void printChar     (ARGL){PRINT_MEM_REF("%c",char,mem,fp);}
static void printWchar_t  (ARGL){PRINT_MEM_REF("%C",wchar_t,mem,fp);}
static void printFloat    (ARGL){PRINT_MEM_REF("%f",float,mem,fp);}
static void printDouble   (ARGL){PRINT_MEM_REF("%lf",double,mem,fp);}
static void printInt8_t   (ARGL){PRINT_MEM_REF("%d",int8_t,mem,fp);}
static void printUint8_t  (ARGL){PRINT_MEM_REF("%u",uint8_t,mem,fp);}
static void printInt16_t  (ARGL){PRINT_MEM_REF("%d",int16_t,mem,fp);}
static void printUint16_t (ARGL){PRINT_MEM_REF("%u",uint16_t,mem,fp);}
static void printInt32_t  (ARGL){PRINT_MEM_REF("%d",int32_t,mem,fp);}
static void printUint32_t (ARGL){PRINT_MEM_REF("%u",uint32_t,mem,fp);}
static void printInt64_t  (ARGL){PRINT_MEM_REF("%ld",int64_t,mem,fp);}
static void printUint64_t (ARGL){PRINT_MEM_REF("%lu",uint64_t,mem,fp);}
static void printIptr     (ARGL){PRINT_MEM_REF("%ld",intptr_t,mem,fp);}
static void printUptr     (ARGL){PRINT_MEM_REF("%lu",uintptr_t,mem,fp);}
static void printVPtr     (ARGL){PRINT_MEM_REF("%p",void*,mem,fp);}
#undef ARGL
#undef PRINT_MEM_REF
static void (*PrintMemoryRefFuncList[])(uint8_t*,FILE*)=
{
	printShortMem ,
	printIntMem   ,
	printUShortMem,
	printUIntMem  ,
	printLongMem  ,
	printULongMem ,
	printLLongMem ,
	printULLongMem,
	printPtrdiff_t,
	printSize_t   ,
	printSsize_t  ,
	printChar     ,
	printWchar_t  ,
	printFloat    ,
	printDouble   ,
	printInt8_t   ,
	printUint8_t  ,
	printInt16_t  ,
    printUint16_t ,
    printInt32_t  ,
    printUint32_t ,
    printInt64_t  ,
    printUint64_t ,
    printIptr     ,
    printUptr     ,
    printVPtr     ,
};
/*-----------------------*/

/*memory caster list*/
#define CAST_TO_I32(TYPE) return FKL_MAKE_VM_I32(*(TYPE*)mem);
#define CAST_TO_I64(TYPE) int64_t t=*(TYPE*)mem;return fklNewVMvalue(FKL_I64,&t,heap);
#define ARGL uint8_t* mem,FklVMheap* heap
static FklVMvalue* castShortMem (ARGL){CAST_TO_I32(short)}
static FklVMvalue* castIntMem   (ARGL){CAST_TO_I32(int)}
static FklVMvalue* castUShortMem(ARGL){CAST_TO_I32(unsigned short)}
static FklVMvalue* castUintMem  (ARGL){CAST_TO_I64(unsigned int)}
static FklVMvalue* castLongMem  (ARGL){CAST_TO_I64(long)}
static FklVMvalue* castULongMem (ARGL){CAST_TO_I64(unsigned long)}
static FklVMvalue* castLLongMem (ARGL){CAST_TO_I64(long long)}
static FklVMvalue* castULLongMem(ARGL){CAST_TO_I64(unsigned long long)}
static FklVMvalue* castPtrdiff_t(ARGL){CAST_TO_I64(ptrdiff_t)}
static FklVMvalue* castSize_t   (ARGL){CAST_TO_I64(size_t)}
static FklVMvalue* castSsize_t  (ARGL){CAST_TO_I64(ssize_t)}
static FklVMvalue* castChar     (ARGL){return FKL_MAKE_VM_CHR(*(char*)mem);}
static FklVMvalue* castWchar_t  (ARGL){return FKL_MAKE_VM_I32(*(wchar_t*)mem);}
static FklVMvalue* castFloat    (ARGL){double t=*(float*)mem;return fklNewVMvalue(FKL_F64,&t,heap);}
static FklVMvalue* castDouble   (ARGL){double t=*(double*)mem;return fklNewVMvalue(FKL_F64,&t,heap);}
static FklVMvalue* castInt8_t   (ARGL){CAST_TO_I32(int8_t)}
static FklVMvalue* castUint8_t  (ARGL){CAST_TO_I32(uint8_t)}
static FklVMvalue* castInt16_t  (ARGL){CAST_TO_I32(int16_t)}
static FklVMvalue* castUint16_t (ARGL){CAST_TO_I32(uint16_t)}
static FklVMvalue* castInt32_t  (ARGL){CAST_TO_I32(int32_t)}
static FklVMvalue* castUint32_t (ARGL){CAST_TO_I32(uint32_t)}
static FklVMvalue* castInt64_t  (ARGL){CAST_TO_I64(int64_t)}
static FklVMvalue* castUint64_t (ARGL){CAST_TO_I64(uint64_t)}
static FklVMvalue* castIptr     (ARGL){CAST_TO_I64(intptr_t)}
static FklVMvalue* castUptr     (ARGL){CAST_TO_I64(uintptr_t)}
static FklVMvalue* castVPtr     (ARGL){return fklNewVMvalue(FKL_I64,mem,heap);}
#undef ARGL
#undef CAST_TO_I32
#undef CAST_TO_I64
static FklVMvalue*(*MemoryCasterList[])(uint8_t*,FklVMheap*)=
{
	castShortMem ,
	castIntMem   ,
	castUShortMem,
	castUintMem  ,
	castLongMem  ,
	castULongMem ,
	castLLongMem ,
	castULLongMem,
	castPtrdiff_t,
	castSize_t   ,
	castSsize_t  ,
	castChar     ,
	castWchar_t  ,
	castFloat    ,
	castDouble   ,
	castInt8_t   ,
	castUint8_t  ,
	castInt16_t  ,
    castUint16_t ,
    castInt32_t  ,
    castUint32_t ,
    castInt64_t  ,
    castUint64_t ,
    castIptr     ,
    castUptr     ,
    castVPtr     ,
};
/*------------------*/

/*Memory setting function list*/
#define BODY(EXPRESSION,TYPE,VALUE) if(EXPRESSION)return 1;*(TYPE*)mem=(VALUE);return 0;
#define SET_NUM(TYPE) BODY(!FKL_IS_I32(v)&&!FKL_IS_I64(v),TYPE,FKL_IS_I32(v)?FKL_GET_I32(v):v->u.i64)
#define ARGL uint8_t* mem,FklVMvalue* v
static int setShortMem (ARGL){SET_NUM(short)}
static int setIntMem   (ARGL){SET_NUM(int)}
static int setUShortMem(ARGL){SET_NUM(unsigned short)}
static int setUintMem  (ARGL){SET_NUM(unsigned int)}
static int setLongMem  (ARGL){SET_NUM(long)}
static int setULongMem (ARGL){SET_NUM(unsigned long)}
static int setLLongMem (ARGL){SET_NUM(long long)}
static int setULLongMem(ARGL){SET_NUM(unsigned long long)}
static int setPtrdiff_t(ARGL){SET_NUM(ptrdiff_t)}
static int setSize_t   (ARGL){SET_NUM(size_t)}
static int setSsize_t  (ARGL){SET_NUM(ssize_t)}
static int setChar     (ARGL){BODY(!FKL_IS_CHR(v),char,FKL_GET_CHR(v))}
static int setWchar_t  (ARGL){BODY(!FKL_IS_CHR(v)&&!FKL_IS_I32(v),wchar_t,FKL_IS_I32(v)?FKL_GET_I32(v):FKL_GET_CHR(v))}
static int setFloat    (ARGL){BODY(!FKL_IS_F64(v)&&!FKL_IS_I32(v)&&FKL_IS_I64(v),float,FKL_IS_F64(v)?v->u.f64:(FKL_IS_I32(v)?FKL_GET_I32(v):v->u.i64))}
static int setDouble   (ARGL){BODY(!FKL_IS_F64(v)&&!FKL_IS_I32(v)&&FKL_IS_I64(v),double,FKL_IS_F64(v)?v->u.f64:(FKL_IS_I32(v)?FKL_GET_I32(v):v->u.i64))}
static int setInt8_t   (ARGL){SET_NUM(int8_t)}
static int setUint8_t  (ARGL){SET_NUM(uint8_t)}
static int setInt16_t  (ARGL){SET_NUM(int16_t)}
static int setUint16_t (ARGL){SET_NUM(uint16_t)}
static int setInt32_t  (ARGL){SET_NUM(int32_t)}
static int setUint32_t (ARGL){SET_NUM(uint32_t)}
static int setInt64_t  (ARGL){SET_NUM(int64_t)}
static int setUint64_t (ARGL){SET_NUM(uint64_t)}
static int setIptr     (ARGL){SET_NUM(intptr_t)}
static int setUptr     (ARGL){SET_NUM(uintptr_t)}
static int setVPtr     (ARGL){SET_NUM(uintptr_t)}
#undef SET_NUM
#undef BODY
static int (*MemorySeterList[])(ARGL)=
{
	setShortMem ,
	setIntMem   ,
	setUShortMem,
	setUintMem  ,
	setLongMem  ,
	setULongMem ,
	setLLongMem ,
	setULLongMem,
	setPtrdiff_t,
	setSize_t   ,
	setSsize_t  ,
	setChar     ,
	setWchar_t  ,
	setFloat    ,
	setDouble   ,
	setInt8_t   ,
	setUint8_t  ,
	setInt16_t  ,
    setUint16_t ,
    setInt32_t  ,
    setUint32_t ,
    setInt64_t  ,
    setUint64_t ,
    setIptr     ,
    setUptr     ,
    setVPtr     ,
};
#undef ARGL
/*------------------*/

void fklPrepFFIcif(ffi_cif* cif,int argc,ffi_type** atypes,ffi_type* rtype)
{
	pthread_mutex_lock(&GPrepCifLock);
	ffi_status r=ffi_prep_cif(cif,FFI_DEFAULT_ABI,argc,rtype,atypes);
	pthread_mutex_unlock(&GPrepCifLock);
	FKL_ASSERT(r==FFI_OK,__func__);
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

void fklApplyFlproc(FklVMflproc* f,void* rvalue,void** avalue)
{
	ffi_call(&f->cif,f->func,rvalue,avalue);
}

ffi_type* fklGetFfiType(FklTypeId_t type)
{
	if(type>fklGetLastNativeTypeId())
		type=fklGetLastNativeTypeId();
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
			FKL_ASSERT(p,__func__);
			*t=v->u.flproc->func;
			*p=t;
		}
		return 0;
	}
	else if(type==0||type>fklGetLastNativeTypeId())
		type=fklGetLastNativeTypeId();;
	return fklCastValueToVptrFunctionsList[type-1](v,p);
}

void fklPrintMemoryRef(FklTypeId_t type,FklVMmem* mem,FILE* fp)
{
	PrintMemoryRefFuncList[type-1](mem->mem,fp);
}

FklVMvalue* fklMemoryCast(FklTypeId_t type,FklVMmem* mem,FklVMheap* heap)
{
	return MemoryCasterList[type-1](mem->mem,heap);
}

int fklMemorySet(FklTypeId_t type,FklVMmem* mem,FklVMvalue* v)
{
	return MemorySeterList[type-1](mem->mem,v);
}

FklVMmem* fklNewVMmem(FklTypeId_t type,FklVMmemMode mode,uint8_t* mem)
{
	FklVMmem* tmp=(FklVMmem*)malloc(sizeof(FklVMmem));
	FKL_ASSERT(tmp,__func__);
	tmp->type=type;
	tmp->mode=mode;
	tmp->mem=mem;
	return tmp;
}


