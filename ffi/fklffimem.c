#include"fklffimem.h"
#include"fklffidll.h"
#include<fakeLisp/utils.h>
#include<string.h>
#include<fakeLisp/fklni.h>
#include<float.h>
#ifdef _WIN32
#include<BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

static FklVMvalue* FfiRel=NULL;
FklSid_t FfiMemUdSid=0;
static FklSid_t FfiAtomicSid=0;
static FklSid_t FfiRawSid=0;
static FklSid_t FfiRefSid=0;
static FklSid_t FfiDeRefSid=0;

static void _mem_finalizer(void* p)
{
	free(p);
}

static void _mem_atomic_finalizer(void* p)
{
	FklFfiMem* m=p;
	free(m->mem);
	free(p);
}

#define PRINT_MEM_REF(FMT,TYPE,MEM,FP) fprintf(FP,FMT,*(TYPE*)(MEM))
#define ARGL FILE* fp,void* mem
static void printShortMem (ARGL){PRINT_MEM_REF("%hd",short,mem,fp);}
static void printIntMem   (ARGL){PRINT_MEM_REF("%d",int,mem,fp);}
static void printUShortMem(ARGL){PRINT_MEM_REF("%hu",unsigned short,mem,fp);}
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
static void printVptr     (ARGL){PRINT_MEM_REF("%p",void*,mem,fp);}
#undef ARGL
#undef PRINT_MEM_REF

#define ARGL FklFfiMem* mem
#define GET_INTEGER_MEM(TYPE) return *(TYPE*)(mem->mem);
static int64_t __get_integer_from_short    (ARGL){GET_INTEGER_MEM(short)}
static int64_t __get_integer_from_int      (ARGL){GET_INTEGER_MEM(int)}
static int64_t __get_integer_from_ushort   (ARGL){GET_INTEGER_MEM(unsigned short)}
static int64_t __get_integer_from_unsigned (ARGL){GET_INTEGER_MEM(unsigned)}
static int64_t __get_integer_from_long     (ARGL){GET_INTEGER_MEM(long)}
static int64_t __get_integer_from_ulong    (ARGL){GET_INTEGER_MEM(unsigned long)}
static int64_t __get_integer_from_longlong (ARGL){GET_INTEGER_MEM(long long)}
static int64_t __get_integer_from_ulonglong(ARGL){GET_INTEGER_MEM(unsigned long)}
static int64_t __get_integer_from_ptrdiff_t(ARGL){GET_INTEGER_MEM(ptrdiff_t)}
static int64_t __get_integer_from_size_t   (ARGL){GET_INTEGER_MEM(size_t)}
static int64_t __get_integer_from_ssize_t  (ARGL){GET_INTEGER_MEM(ssize_t)}
static int64_t __get_integer_from_char     (ARGL){GET_INTEGER_MEM(char)}
static int64_t __get_integer_from_wchar_t  (ARGL){GET_INTEGER_MEM(wchar_t)}
static int64_t __get_integer_from_float    (ARGL){GET_INTEGER_MEM(float)}
static int64_t __get_integer_from_double   (ARGL){GET_INTEGER_MEM(double)}
static int64_t __get_integer_from_int8_t   (ARGL){GET_INTEGER_MEM(int8_t)}
static int64_t __get_integer_from_uint8_t  (ARGL){GET_INTEGER_MEM(uint8_t)}
static int64_t __get_integer_from_int16_t  (ARGL){GET_INTEGER_MEM(int16_t)}
static int64_t __get_integer_from_uint16_t (ARGL){GET_INTEGER_MEM(uint16_t)}
static int64_t __get_integer_from_int32    (ARGL){GET_INTEGER_MEM(int32_t)}
static int64_t __get_integer_from_uint32_t (ARGL){GET_INTEGER_MEM(uint32_t)}
static int64_t __get_integer_from_int64_t  (ARGL){GET_INTEGER_MEM(int64_t)}
static int64_t __get_integer_from_uint64_t (ARGL){GET_INTEGER_MEM(uint64_t)}
static int64_t __get_integer_from_iptr     (ARGL){GET_INTEGER_MEM(intptr_t)}
static int64_t __get_integer_from_uptr     (ARGL){GET_INTEGER_MEM(uintptr_t)}
#undef GET_INTEGER_MEM
#define GET_DOUBLE_MEM(TYPE) return *(TYPE*)(mem->mem);
static double __get_double_from_short    (ARGL){GET_DOUBLE_MEM(short)}
static double __get_double_from_int      (ARGL){GET_DOUBLE_MEM(int)}
static double __get_double_from_uShort   (ARGL){GET_DOUBLE_MEM(unsigned short)}
static double __get_double_from_unsigned (ARGL){GET_DOUBLE_MEM(unsigned)}
static double __get_double_from_long     (ARGL){GET_DOUBLE_MEM(long)}
static double __get_double_from_ulong    (ARGL){GET_DOUBLE_MEM(unsigned long)}
static double __get_double_from_longlong (ARGL){GET_DOUBLE_MEM(long long)}
static double __get_double_from_ulonglong(ARGL){GET_DOUBLE_MEM(unsigned long)}
static double __get_double_from_ptrdiff_t(ARGL){GET_DOUBLE_MEM(ptrdiff_t)}
static double __get_double_from_size_t   (ARGL){GET_DOUBLE_MEM(size_t)}
static double __get_double_from_ssize_t  (ARGL){GET_DOUBLE_MEM(ssize_t)}
static double __get_double_from_char     (ARGL){GET_DOUBLE_MEM(char)}
static double __get_double_from_wchar_t  (ARGL){GET_DOUBLE_MEM(wchar_t)}
static double __get_double_from_float    (ARGL){GET_DOUBLE_MEM(float)}
static double __get_double_from_double   (ARGL){GET_DOUBLE_MEM(double)}
static double __get_double_from_int8_t   (ARGL){GET_DOUBLE_MEM(int8_t)}
static double __get_double_from_uint8_t  (ARGL){GET_DOUBLE_MEM(uint8_t)}
static double __get_double_from_int16_t  (ARGL){GET_DOUBLE_MEM(int16_t)}
static double __get_double_from_uint16_t (ARGL){GET_DOUBLE_MEM(uint16_t)}
static double __get_double_from_int32    (ARGL){GET_DOUBLE_MEM(int32_t)}
static double __get_double_from_uint32_t (ARGL){GET_DOUBLE_MEM(uint32_t)}
static double __get_double_from_int64_t  (ARGL){GET_DOUBLE_MEM(int64_t)}
static double __get_double_from_uint64_t (ARGL){GET_DOUBLE_MEM(uint64_t)}
static double __get_double_from_iptr     (ARGL){GET_DOUBLE_MEM(intptr_t)}
static double __get_double_from_uptr     (ARGL){GET_DOUBLE_MEM(uintptr_t)}
#undef GET_DOUBLE_MEM
static int64_t (*__ffiGetIntegerFuncList[])(ARGL)=
{
	NULL                        ,
	__get_integer_from_short    ,
	__get_integer_from_int      ,
	__get_integer_from_ushort   ,
	__get_integer_from_unsigned ,
	__get_integer_from_long     ,
	__get_integer_from_ulong    ,
	__get_integer_from_longlong ,
	__get_integer_from_ulonglong,
	__get_integer_from_ptrdiff_t,
	__get_integer_from_size_t   ,
	__get_integer_from_ssize_t  ,
	__get_integer_from_char     ,
	__get_integer_from_wchar_t  ,
	__get_integer_from_float    ,
	__get_integer_from_double   ,
	__get_integer_from_int8_t   ,
	__get_integer_from_uint8_t  ,
	__get_integer_from_int16_t  ,
	__get_integer_from_uint16_t ,
	__get_integer_from_int32    ,
	__get_integer_from_uint32_t ,
	__get_integer_from_int64_t  ,
	__get_integer_from_uint64_t ,
	__get_integer_from_iptr     ,
	__get_integer_from_uptr     ,
};
static double (*__ffiGetDoubleFuncList[])(ARGL)=
{
	NULL                  ,
	__get_double_from_short    ,
	__get_double_from_int      ,
	__get_double_from_uShort   ,
	__get_double_from_unsigned ,
	__get_double_from_long     ,
	__get_double_from_ulong    ,
	__get_double_from_longlong ,
	__get_double_from_ulonglong,
	__get_double_from_ptrdiff_t,
	__get_double_from_size_t   ,
	__get_double_from_ssize_t  ,
	__get_double_from_char     ,
	__get_double_from_wchar_t  ,
	__get_double_from_float    ,
	__get_double_from_double   ,
	__get_double_from_int8_t   ,
	__get_double_from_uint8_t  ,
	__get_double_from_int16_t  ,
	__get_double_from_uint16_t ,
	__get_double_from_int32    ,
	__get_double_from_uint32_t ,
	__get_double_from_int64_t  ,
	__get_double_from_uint64_t ,
	__get_double_from_iptr     ,
	__get_double_from_uptr     ,
};
#undef ARGL

static void (*NativeTypePrinterList[])(FILE*,void*)=
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
	printVptr     ,
};

static void _mem_print(void* p,FILE* fp)
{
	FklFfiMem* m=p;
	if(fklFfiIsNativeTypeId(m->type))
		NativeTypePrinterList[m->type-1](fp,m->mem);
	if(fklFfiIsPtrTypeId(m->type)||fklFfiIsFunctionTypeId(m->type))
		fprintf(fp,"%p",*((void**)(m->mem)));
	else if(m->type==FKL_FFI_TYPE_FILE_P)
		fprintf(fp,"%p",*((void**)(m->mem)));
	else if(m->type==FKL_FFI_TYPE_STRING)
		fprintf(fp,"%s",(char*)m->mem);
	else if(fklFfiIsStructTypeId(m->type))
	{
		FklDefStructType* st=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
		fputc('{',fp);
		size_t offset=0;
		for(FklHashTableNodeList* list=st->layout->list;list;list=list->next)
		{
			FklFfiMemberHashItem* item=list->node->item;
			FklDefTypeUnion tu=fklFfiGetTypeUnion(item->type);
			size_t align=fklFfiGetTypeAlign(tu);
			offset+=(offset%align)?align-offset%align:0;
			fputc('.',fp);
			fklPrintString(fklGetGlobSymbolWithId(item->key)->symbol,fp);
			fputc('=',fp);
			FklFfiMem tm={item->type,m->mem+offset};
			_mem_print(&tm,fp);
			size_t memberSize=fklFfiGetTypeSize(tu);
			offset+=memberSize;
			fputc(',',fp);
		}
		fputc('}',fp);
	}
	else if(fklFfiIsUnionTypeId(m->type))
	{
		FklDefUnionType* ut=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
		fputc('{',fp);
		for(FklHashTableNodeList* list=ut->layout->list;list;list=list->next)
		{
			FklFfiMemberHashItem* item=list->node->item;
			fputc('.',fp);
			fklPrintString(fklGetGlobSymbolWithId(item->key)->symbol,fp);
			fputc('=',fp);
			FklFfiMem tm={item->type,m->mem};
			_mem_print(&tm,fp);
			fputc(';',fp);
		}
		fputc('}',fp);
	}
	else if(fklFfiIsArrayTypeId(m->type))
	{
		FklDefArrayType* at=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
		fputc('[',fp);
		for(uint32_t i=0;i<at->num;i++)
		{
			FklFfiMem tm={at->etype,m->mem+i*fklFfiGetTypeSizeWithTypeId(at->etype)};
			_mem_print(&tm,fp);
			fputc(',',fp);
		}
		fputc(']',fp);
	}
}

int _mem_equal(const FklVMudata* a,const FklVMudata* b)
{
	if(a->t->__call!=b->t->__call)
		return 0;
	if(a->t->__call)
	{
		FklFfiProc* p0=a->data;
		FklFfiProc* p1=b->data;
		if(p0->type==p1->type&&p0->func==p1->func)
			return 1;
	}
	else
	{
		FklFfiMem* m0=a->data;
		FklFfiMem* m1=b->data;
		if(m0->type==m1->type)
		{
			if((m0->type==FKL_FFI_TYPE_STRING||m0->type==FKL_FFI_TYPE_FILE_P)&&m0->mem==m1->mem)
				return 1;
			else if(!memcmp(m0->mem,m1->mem,fklFfiGetTypeSizeWithTypeId(m0->type)))
				return 1;
		}
	}
	return 0;
}

static int _mem_cmp_VU(FklVMvalue* a,FklFfiMem* b,int* isUnableToBeCmp)
{
	if((FKL_IS_STR(a)&&b->type!=FKL_FFI_TYPE_STRING)
			||(!FKL_IS_STR(a)&&b->type==FKL_FFI_TYPE_STRING)
			||!fklIsVMnumber(a)
			||!fklFfiIsNumTypeId(b->type))
	{
		*isUnableToBeCmp=1;
		return 0;
	}
	if(b->type==FKL_FFI_TYPE_STRING)
	{
		size_t len=strlen(b->mem);
		int r=memcmp(a->u.str->str,b->mem,a->u.str->size<len?a->u.str->size:len);
		if(!r)
			return (int64_t)a->u.str->size-(int64_t)len;
		return r;
	}
	else if(FKL_IS_F64(a)||b->type==FKL_FFI_TYPE_DOUBLE)
	{
		double ad=fklGetDouble(a);
		double bd=__ffiGetDoubleFuncList[b->type](b);
		if(ad-bd>DBL_EPSILON)
			return 1;
		else if(ad-bd<-DBL_EPSILON)
			return -1;
		else
			return 0;
	}
	else if(fklIsFixint(a))
	{
		int64_t ai=fklGetInt(a);
		int64_t bi=__ffiGetIntegerFuncList[b->type](b);
		if(ai>bi)
			return 1;
		else if(ai<bi)
			return -1;
		else
			return 0;
	}
	else if(FKL_IS_BIG_INT(a))
	{
		if(fklIsLtI64MinBigInt(a->u.bigInt))
			return -1;
		else if(fklIsGtI64MaxBigInt(a->u.bigInt))
			return 1;
		else
		{
			int64_t ai=fklBigIntToI64(a->u.bigInt);
			int64_t bi=__ffiGetIntegerFuncList[b->type](b);
			if(ai>bi)
				return 1;
			else if(ai<bi)
				return -1;
			else
				return 0;
		}
	}
	return 0;
}

static int _mem_cmp_UU(FklFfiMem* a,FklFfiMem* b,int* isUnableToBeCmp)
{
	if((a->type==FKL_FFI_TYPE_STRING&&b->type!=a->type)
			||(a->type!=FKL_FFI_TYPE_STRING&&b->type==FKL_FFI_TYPE_STRING)
			||!fklFfiIsNumTypeId(a->type)
			||!fklFfiIsNumTypeId(b->type))
	{
		*isUnableToBeCmp=1;
		return 0;
	}
	if(a->type==FKL_FFI_TYPE_STRING&&b->type==FKL_FFI_TYPE_STRING)
		return strcmp(a->mem,b->mem);
	else if(fklFfiIsIntegerTypeId(a->type)&&fklFfiIsIntegerTypeId(b->type))
	{
		int64_t ai=__ffiGetIntegerFuncList[a->type](a);
		int64_t bi=__ffiGetIntegerFuncList[b->type](b);
		if(ai>bi)
			return 1;
		else if(ai<bi)
			return -1;
		else
			return 0;
	}
	else
	{
		double ad=__ffiGetDoubleFuncList[a->type](a);
		double bd=__ffiGetDoubleFuncList[b->type](b);
		if(ad-bd>DBL_EPSILON)
			return 1;
		else if(ad-bd<-DBL_EPSILON)
			return -1;
		else
			return 0;
	}
	return 0;
}

static int _mem_cmp(FklVMvalue* a,FklVMvalue* b,int* isUnableToBeCmp)
{
	if(fklFfiIsMem(a)&&!fklFfiIsMem(b))
		return _mem_cmp_VU(b,a->u.ud->data,isUnableToBeCmp)*-1;
	else if(fklFfiIsMem(b)&&!fklFfiIsMem(a))
		return _mem_cmp_VU(a,b->u.ud->data,isUnableToBeCmp);
	else if(fklFfiIsMem(a)&&fklFfiIsMem(b))
		return _mem_cmp_UU(a->u.ud->data,b->u.ud->data,isUnableToBeCmp);
	else
	{
		*isUnableToBeCmp=1;
		return 0;
	}
}

static FklVMudMethodTable FfiMemMethodTable=
{
	.__princ=_mem_print,
	.__prin1=_mem_print,
	.__finalizer=_mem_finalizer,
	.__equal=_mem_equal,
	.__call=NULL,
	.__cmp=_mem_cmp,
	.__write=NULL,
	.__atomic=NULL,
	.__append=NULL,
	.__copy=NULL,
	.__hash=NULL,
};

static FklVMudMethodTable FfiAtomicMemMethodTable=
{
	.__princ=_mem_print,
	.__prin1=_mem_print,
	.__finalizer=_mem_atomic_finalizer,
	.__equal=_mem_equal,
	.__call=NULL,
	.__cmp=_mem_cmp,
	.__write=NULL,
	.__atomic=NULL,
	.__append=NULL,
	.__copy=NULL,
	.__hash=NULL,
};

void fklFfiMemInit(FklVMvalue* rel)
{
	FfiMemUdSid=fklAddSymbolToGlobCstr("ffi-mem")->id;
	FfiAtomicSid=fklAddSymbolToGlobCstr("atomic")->id;
	FfiRawSid=fklAddSymbolToGlobCstr("raw")->id;
	FfiRefSid=fklAddSymbolToGlobCstr("&")->id;
	FfiDeRefSid=fklAddSymbolToGlobCstr("*")->id;
	FfiRel=rel;
}

FklFfiMem* fklFfiCreateMem(FklTypeId_t type,size_t size)
{
	FklFfiMem* r=(FklFfiMem*)malloc(sizeof(FklFfiMem));
	FKL_ASSERT(r);
	r->type=type;
	void* p=calloc(sizeof(uint8_t),size);
	FKL_ASSERT(p);
	r->mem=p;
	return r;
}

FklFfiMem* fklFfiCreateRef(FklTypeId_t type,void* ref)
{
	FklFfiMem* r=(FklFfiMem*)malloc(sizeof(FklFfiMem));
	FKL_ASSERT(r);
	r->type=type;
	r->mem=ref;
	return r;
}

FklVMudata* fklFfiCreateMemUd(FklTypeId_t type,size_t size,FklVMvalue* atomic)
{
	if(atomic==NULL||FKL_GET_SYM(atomic)==FfiAtomicSid)
		return fklCreateVMudata(FfiMemUdSid,&FfiAtomicMemMethodTable,fklFfiCreateMem(type,size),FfiRel);
	else if(FKL_GET_SYM(atomic)==FfiRawSid)
		return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiCreateMem(type,size),FfiRel);
	else
		return NULL;
}

FklVMudata* fklFfiCreateMemRefUd(FklTypeId_t type,void* mem)
{
	return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiCreateRef(type,mem),FfiRel);
}

FklVMudata* fklFfiCreateMemRefUdWithSI(FklFfiMem* m,FklVMvalue* selector,FklVMvalue* pindex)
{
	if(selector==NULL||selector==FKL_VM_NIL)
	{
		if(pindex&&(!fklFfiIsPtrTypeId(m->type)&&!fklFfiIsArrayTypeId(m->type)&&m->type!=FKL_FFI_TYPE_STRING))
			return NULL;
		if(pindex&&(fklFfiIsPtrTypeId(m->type)||fklFfiIsArrayTypeId(m->type)||m->type==FKL_FFI_TYPE_STRING))
		{
			int64_t index=fklGetInt(pindex);
			if(fklFfiIsPtrTypeId(m->type))
			{
				FklDefPtrType* pt=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				void* p=*(void**)m->mem;
				return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiCreateRef(pt->ptype,p+index*fklFfiGetTypeSizeWithTypeId(pt->ptype)),FfiRel);
			}
			else if(fklFfiIsArrayTypeId(m->type))
			{
				FklDefArrayType* at=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiCreateRef(at->etype,m->mem+index*fklFfiGetTypeSizeWithTypeId(at->etype)),FfiRel);
			}
			else
				return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiCreateRef(FKL_FFI_TYPE_CHAR,m->mem+index*sizeof(char)),FfiRel);
		}
		else
			return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiCreateRef(m->type,m->mem),FfiRel);
	}
	else
	{
		FklSid_t selectorId=FKL_GET_SYM(selector);
		if(selectorId==FfiRefSid)
		{
			FklFfiMem* ptr=fklFfiCreateMem(fklFfiCreatePtrType(m->type),sizeof(void*));
			*(void**)ptr->mem=m->mem;
			return fklCreateVMudata(FfiMemUdSid,&FfiAtomicMemMethodTable,ptr,FfiRel);
		}
		else if(selectorId==FfiDeRefSid)
		{
			if(!fklFfiIsPtrTypeId(m->type))
				return NULL;
			FklDefPtrType* ptrType=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
			FklFfiMem* deref=fklFfiCreateRef(ptrType->ptype,*(void**)m->mem);
			return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,deref,FfiRel);
		}
		else if(fklFfiIsStructTypeId(m->type)||fklFfiIsUnionTypeId(m->type))
		{
			FklHashTable* layout=NULL;
			if(fklFfiIsStructTypeId(m->type))
			{
				FklDefStructType* st=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				layout=st->layout;
			}
			else
			{
				FklDefUnionType* ut=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				layout=ut->layout;
			}
			FklFfiMemberHashItem* item=fklGetHashItem(&selectorId,layout);
			return fklCreateVMudata(FfiMemUdSid
					,&FfiMemMethodTable
					,fklFfiCreateRef(item->type,m->mem+item->offset),FfiRel);
		}
	}
	return NULL;
}

int fklFfiIsMem(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)
		&&p->u.ud->type==FfiMemUdSid
		&&(p->u.ud->t==&FfiMemMethodTable||p->u.ud->t==&FfiAtomicMemMethodTable);
}

static int __ffiGetVMvalueAsF64(FklVMvalue* val,double* d)
{
	if(fklIsInt(val))
		*d=fklGetInt(val);
	else if(FKL_IS_CHR(val))
		*d=FKL_GET_CHR(val);
	else if(FKL_IS_F64(val))
		*d=val->u.f64;
	else if(fklFfiIsMem(val)&&fklFfiIsNumTypeId(((FklFfiMem*)val->u.ud->data)->type))
		*d=__ffiGetDoubleFuncList[((FklFfiMem*)val->u.ud->data)->type](val->u.ud->data);
	else
		return 1;
	return 0;
}

static int __ffiGetVMvalueAsI64(FklVMvalue* val,int64_t* i)
{
	if(fklIsInt(val))
		*i=fklGetInt(val);
	else if(FKL_IS_CHR(val))
		*i=FKL_GET_CHR(val);
	else if(FKL_IS_F64(val))
		*i=val->u.f64;
	else if(fklFfiIsMem(val)&&fklFfiIsNumTypeId(((FklFfiMem*)val->u.ud->data)->type))
		*i=__ffiGetIntegerFuncList[((FklFfiMem*)val->u.ud->data)->type](val->u.ud->data);
	else
		return 1;
	return 0;
}

#define ARGL void* mem,FklVMvalue* val
#define SET_INTEGER_MEM(TYPE) int64_t i=0.0;if(__ffiGetVMvalueAsI64(val,&i))return 1;*(TYPE*)mem=i;return 0;
#define SET_FLOAT_MEM(TYPE) double d=0.0;if(__ffiGetVMvalueAsF64(val,&d))return 1;*(TYPE*)mem=d;return 0;
static int __set_short    (ARGL){SET_INTEGER_MEM(short)}
static int __set_int      (ARGL){SET_INTEGER_MEM(int)}
static int __set_ushort   (ARGL){SET_INTEGER_MEM(unsigned short)}
static int __set_unsigned (ARGL){SET_INTEGER_MEM(unsigned)}
static int __set_long     (ARGL){SET_INTEGER_MEM(long)}
static int __set_ulong    (ARGL){SET_INTEGER_MEM(unsigned long)}
static int __set_longlong (ARGL){SET_INTEGER_MEM(long long)}
static int __set_ulonglong(ARGL){SET_INTEGER_MEM(unsigned long long)}
static int __set_ptrdiff_t(ARGL){SET_INTEGER_MEM(ptrdiff_t)}
static int __set_size_t   (ARGL){SET_INTEGER_MEM(size_t)}
static int __set_ssize_t  (ARGL){SET_INTEGER_MEM(ssize_t)}
static int __set_char     (ARGL){SET_INTEGER_MEM(char)}
static int __set_wchar_t  (ARGL){SET_INTEGER_MEM(wchar_t)}
static int __set_float    (ARGL){SET_FLOAT_MEM(float)}
static int __set_double   (ARGL){SET_FLOAT_MEM(double)}
static int __set_int8_t   (ARGL){SET_INTEGER_MEM(int8_t)}
static int __set_uint8_t  (ARGL){SET_INTEGER_MEM(uint8_t)}
static int __set_int16_t  (ARGL){SET_INTEGER_MEM(int16_t)}
static int __set_uint16_t (ARGL){SET_INTEGER_MEM(uint16_t)}
static int __set_int32_t  (ARGL){SET_INTEGER_MEM(int32_t)}
static int __set_uint32_t (ARGL){SET_INTEGER_MEM(uint32_t)}
static int __set_int64_t  (ARGL){SET_INTEGER_MEM(int64_t)}
static int __set_uint64_t (ARGL){SET_INTEGER_MEM(uint64_t)}
static int __set_iptr     (ARGL){SET_INTEGER_MEM(intptr_t)}
static int __set_uptr     (ARGL){SET_INTEGER_MEM(uintptr_t)}
#undef SET_INTEGER_MEM
#undef SET_FLOAT_MEM
#undef ARGL
static int (*__ffiMemSetList[])(void*,FklVMvalue*)=
{
	NULL           ,
	__set_short    ,
	__set_int      ,
	__set_ushort   ,
	__set_unsigned ,
	__set_long     ,
	__set_ulong    ,
	__set_longlong ,
	__set_ulonglong,
	__set_ptrdiff_t,
	__set_size_t   ,
	__set_ssize_t  ,
	__set_char     ,
	__set_wchar_t  ,
	__set_float    ,
	__set_double   ,
	__set_int8_t   ,
	__set_uint8_t  ,
	__set_int16_t  ,
	__set_uint16_t ,
	__set_int32_t  ,
	__set_uint32_t ,
	__set_int64_t  ,
	__set_uint64_t ,
	__set_iptr     ,
	__set_uptr     ,
};

int fklFfiSetMemForProc(FklVMudata* ud,FklVMvalue* val)
{
	FklFfiMem* ref=ud->data;
	if(fklFfiIsNumTypeId(ref->type))
		return __ffiMemSetList[ref->type](ref->mem,val);
	else if(fklFfiIsPtrTypeId(ref->type))
	{
		if(val==FKL_VM_NIL)
			*(void**)ref->mem=NULL;
		else if(fklFfiIsMem(val))
		{
			FklFfiMem* valmem=val->u.ud->data;
			if(!fklFfiIsPtrTypeId(valmem->type)&&valmem->type!=FKL_FFI_TYPE_STRING&&valmem->type!=FKL_FFI_TYPE_FILE_P)
				return 1;
			if(fklFfiIsPtrTypeId(valmem->type))
				*(void**)ref->mem=*(void**)valmem->mem;
			else if(valmem->type==FKL_FFI_TYPE_STRING)
				*(void**)ref->mem=valmem->mem;
			else if(valmem->type==FKL_FFI_TYPE_FILE_P)
				*(void**)ref->mem=valmem->mem;
			else
				return 1;
		}
		else if(fklFfiIsProc(val))
		{
			FklFfiProc* valproc=val->u.ud->data;
			*(void**)ref->mem=valproc->func;
		}
		else
			return 1;
	}
	else if(ref->type==FKL_FFI_TYPE_STRING)
	{
		if(FKL_IS_STR(val))
		{
			if(ref->mem)
				free(ref->mem);
			ref->mem=fklCharBufToCstr(val->u.str->str,val->u.str->size);
		}
		else if(FKL_IS_SYM(val))
		{
			if(ref->mem)
				free(ref->mem);
			FklString* s=fklGetGlobSymbolWithId(FKL_GET_SYM(val))->symbol;
			ref->mem=fklCharBufToCstr(s->str,s->size);
		}
		else if(fklFfiIsMem(val))
		{
			FklFfiMem* m=val->u.ud->data;
			if(m->type!=FKL_FFI_TYPE_STRING)
				return 1;
			free(ref->mem);
			ref->mem=m->mem;
			ud->t=&FfiMemMethodTable;
		}
		else
			return 1;
	}
	else if(ref->type==FKL_FFI_TYPE_FILE_P)
	{
		if(FKL_IS_FP(val))
			ref->mem=val->u.fp->fp;
		else
			return 1;
	}
	else
	{
		if(!fklFfiIsMem(val))
			return 1;
		else
		{
			FklFfiMem* valmem=val->u.ud->data;
			if(ref->type!=valmem->type)
				return 1;
			if(fklFfiIsArrayTypeId(ref->type))
			{
				free(ref->mem);
				ref->mem=valmem->mem;
				ud->t=&FfiMemMethodTable;
			}
			else
				memcpy(ref->mem,valmem->mem,fklFfiGetTypeSizeWithTypeId(ref->type));
		}
	}
	return 0;
}

int fklFfiSetMem(FklFfiMem* ref,FklVMvalue* val)
{
	if(fklFfiIsNumTypeId(ref->type))
		return __ffiMemSetList[ref->type](ref->mem,val);
	else if(fklFfiIsPtrTypeId(ref->type))
	{
		if(val==FKL_VM_NIL)
			*(void**)ref->mem=NULL;
		else if(fklFfiIsMem(val))
		{
			FklFfiMem* valmem=val->u.ud->data;
			if(!fklFfiIsPtrTypeId(valmem->type)&&valmem->type!=FKL_FFI_TYPE_STRING&&valmem->type!=FKL_FFI_TYPE_FILE_P)
				return 1;
			if(fklFfiIsPtrTypeId(valmem->type))
				*(void**)ref->mem=*(void**)valmem->mem;
			else if(valmem->type==FKL_FFI_TYPE_STRING)
				*(void**)ref->mem=valmem->mem;
			else if(valmem->type==FKL_FFI_TYPE_FILE_P)
				*(void**)ref->mem=valmem->mem;
			else
				return 1;
		}
		else if(fklFfiIsProc(val))
		{
			FklFfiProc* valproc=val->u.ud->data;
			*(void**)ref->mem=valproc->func;
		}
		else
			return 1;
	}
	else if(ref->type==FKL_FFI_TYPE_STRING)
	{
		if(FKL_IS_STR(val))
		{
			if(ref->mem)
				free(ref->mem);
			ref->mem=fklCharBufToCstr(val->u.str->str,val->u.str->size);
		}
		else if(FKL_IS_SYM(val))
		{
			if(ref->mem)
				free(ref->mem);
			FklString* s=fklGetGlobSymbolWithId(FKL_GET_SYM(val))->symbol;
			ref->mem=fklCharBufToCstr(s->str,s->size);
		}
		else if(fklFfiIsMem(val))
		{
			FklFfiMem* m=val->u.ud->data;
			if(m->type!=FKL_FFI_TYPE_STRING)
				return 1;
			free(ref->mem);
			ref->mem=fklCopyCstr(m->mem);
		}
		else
			return 1;
	}
	else if(ref->type==FKL_FFI_TYPE_FILE_P)
	{
		if(FKL_IS_FP(val))
			ref->mem=val->u.fp->fp;
		else
			return 1;
	}
	else
	{
		if(!fklFfiIsMem(val))
			return 1;
		else
		{
			FklFfiMem* valmem=val->u.ud->data;
			if(ref->type!=valmem->type)
				return 1;
			else
				memcpy(ref->mem,valmem->mem,fklFfiGetTypeSizeWithTypeId(ref->type));
		}
	}
	return 0;
}

int fklFfiIsNull(FklFfiMem* m)
{
	return (m->type==FKL_FFI_TYPE_VPTR||fklFfiIsPtrTypeId(m->type))&&!(*(void**)m->mem);
}

int fklFfiIsCastableVMvalueType(FklVMvalue* v)
{
	return !FKL_IS_PAIR(v)
		&&!FKL_IS_CHAN(v)
		&&!FKL_IS_DLL(v)
		&&!FKL_IS_PROC(v)
		&&!FKL_IS_DLPROC(v)
		&&!FKL_IS_VECTOR(v)
		&&!FKL_IS_ERR(v)
		&&!FKL_IS_CONT(v)
		&&!fklFfiIsMem(v);
}

FklVMudata* fklFfiCastVMvalueIntoMem(FklVMvalue* v)
{
	FklFfiMem* m=NULL;
	FklVMudata* r=NULL;
	if(FKL_IS_I32(v))
	{
		m=fklFfiCreateMem(FKL_FFI_TYPE_INT32_T,sizeof(void*));
		*(int32_t*)m->mem=FKL_GET_I32(v);
	}
	else if(FKL_IS_I64(v))
	{
		m=fklFfiCreateMem(FKL_FFI_TYPE_INT64_T,sizeof(void*));
		*(int64_t*)m->mem=v->u.i64;
	}
	else if(FKL_IS_F64(v))
	{
		m=fklFfiCreateMem(FKL_FFI_TYPE_DOUBLE,sizeof(void*));
		*(double*)m->mem=v->u.f64;
	}
	else if(FKL_IS_CHR(v))
	{
		m=fklFfiCreateMem(FKL_FFI_TYPE_CHAR,sizeof(void*));
		*(char*)m->mem=FKL_GET_CHR(v);
	}
	else if(FKL_IS_STR(v))
		m=fklFfiCreateRef(FKL_FFI_TYPE_STRING,fklCharBufToCstr(v->u.str->str,v->u.str->size));
	else if(FKL_IS_SYM(v))
	{
		FklString* s=fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol;
		m=fklFfiCreateRef(FKL_FFI_TYPE_STRING,fklCharBufToCstr(s->str,s->size));
	}
	else if(FKL_IS_FP(v))
	{
		m=fklFfiCreateMem(FKL_FFI_TYPE_FILE_P,sizeof(void*));
		*(FILE**)m->mem=v->u.fp->fp;
	}
	else if(v==FKL_VM_NIL)
		m=fklFfiCreateMem(FKL_FFI_TYPE_VPTR,sizeof(void*));
	else if(fklFfiIsMem(v))
	{
		if(v->u.ud->t->__call)
		{
			FklFfiProc* proc=v->u.ud->data;
			m=fklFfiCreateRef(proc->type,proc->func);
		}
		else
		{
			FklFfiMem* mem=v->u.ud->data;
			m=fklFfiCreateRef(mem->type,mem->mem);
			return fklCreateVMudata(FfiMemUdSid,&FfiMemMethodTable,m,FfiRel);
		}
	}
	r=fklCreateVMudata(FfiMemUdSid,&FfiAtomicMemMethodTable,m,FfiRel);
	return r;
}

FklSid_t fklFfiGetFfiMemUdSid(void)
{
	return FfiMemUdSid;
}

int fklFfiIsValuableMem(FklFfiMem* mem)
{
	if(fklFfiIsNumTypeId(mem->type)||fklFfiIsNull(mem)||mem->type==FKL_FFI_TYPE_STRING)
		return 1;
	return 0;
}

FklVMvalue* fklFfiCreateVMvalue(FklFfiMem* mem,FklVMstack* stack,FklVMgc* gc)
{
	if(fklFfiIsNull(mem))
		return FKL_VM_NIL;
	else if(mem->type==FKL_FFI_TYPE_STRING)
	{
		FklString* str=fklCreateString(strlen(mem->mem),mem->mem);
		return fklCreateVMvalueToStack(FKL_TYPE_STR,str,stack,gc);
	}
	else
	{
		if(fklFfiIsFloatTypeId(mem->type))
		{
			double d=__ffiGetDoubleFuncList[mem->type](mem);
			return fklCreateVMvalueToStack(FKL_TYPE_F64,&d,stack,gc);
		}
		else
			return fklMakeVMint(__ffiGetIntegerFuncList[mem->type](mem),stack,gc);
	}
}

FklVMvalue* fklFfiGetRel(void)
{
	return FfiRel;
}
