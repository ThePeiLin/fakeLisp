#include"fklffimem.h"
#include<fakeLisp/utils.h>
#include<string.h>

static FklSid_t FfiMemUdSid=0;
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
static void printVPtr     (ARGL){PRINT_MEM_REF("%p",void*,mem,fp);}
#undef ARGL
#undef PRINT_MEM_REF

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
	printVPtr     ,
};

static void _mem_print(FILE* fp,void* p)
{
	FklFfiMem* m=p;
	if(fklFfiIsNativeTypeId(m->type))
		NativeTypePrinterList[m->type-1](fp,m->mem);
	if(fklFfiIsPtrTypeId(m->type)||fklFfiIsFunctionTypeId(m->type))
		fprintf(fp,"%p",*((void**)(m->mem)));
	else if(fklFfiIsStructTypeId(m->type))
	{
		FklDefStructType* st=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
		fputc('{',fp);
		size_t offset=0;
		for(uint32_t i=0;i<st->num;i++)
		{
			FklDefTypeUnion tu=fklFfiGetTypeUnion(st->layout[i].type);
			size_t align=fklFfiGetTypeAlign(tu);
			offset+=(offset%align)?align-offset%align:0;
			fprintf(fp,".%s=",fklGetGlobSymbolWithId(st->layout[i].memberSymbol)->symbol);
			FklFfiMem tm={st->layout[i].type,m->mem+offset};
			_mem_print(fp,&tm);
			size_t memberSize=fklFfiGetTypeSize(tu);
			offset+=memberSize;
			fputc(';',fp);
		}
		fputc('}',fp);
	}
	else if(fklFfiIsUnionTypeId(m->type))
	{
		FklDefStructType* st=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
		fputc('{',fp);
		for(uint32_t i=0;i<st->num;i++)
		{
			fprintf(fp,".%s=",fklGetGlobSymbolWithId(st->layout[i].memberSymbol)->symbol);
			FklFfiMem tm={st->layout[i].type,m->mem};
			_mem_print(fp,&tm);
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
			_mem_print(fp,&tm);
			fputc(',',fp);
		}
		fputc(']',fp);
	}
}

static FklVMudMethodTable FfiMemMethodTable=
{
	.__princ=_mem_print,
	.__prin1=_mem_print,
	.__finalizer=_mem_finalizer,
	.__equal=NULL,
	.__invoke=NULL,
};

static FklVMudMethodTable FfiAtomicMemMethodTable=
{
	.__princ=_mem_print,
	.__prin1=_mem_print,
	.__finalizer=_mem_atomic_finalizer,
	.__equal=NULL,
	.__invoke=NULL,
};

void fklFfiMemInit(void)
{
	FfiMemUdSid=fklAddSymbolToGlob("ffi-mem")->id;
	FfiAtomicSid=fklAddSymbolToGlob("atomic")->id;
	FfiRawSid=fklAddSymbolToGlob("raw")->id;
	FfiRefSid=fklAddSymbolToGlob("&")->id;
	FfiDeRefSid=fklAddSymbolToGlob("*")->id;
}

FklFfiMem* fklFfiNewMem(FklTypeId_t type,size_t size)
{
	FklFfiMem* r=(FklFfiMem*)malloc(sizeof(FklFfiMem));
	FKL_ASSERT(r,__func__);
	r->type=type;
	void* p=calloc(sizeof(uint8_t),size);
	FKL_ASSERT(p,__func__);
	r->mem=p;
	return r;
}

FklFfiMem* fklFfiNewRef(FklTypeId_t type,void* ref)
{
	FklFfiMem* r=(FklFfiMem*)malloc(sizeof(FklFfiMem));
	FKL_ASSERT(r,__func__);
	r->type=type;
	r->mem=ref;
	return r;
}

FklVMudata* fklFfiNewMemUd(FklTypeId_t type,size_t size,FklVMvalue* atomic)
{
	if(atomic==NULL||FKL_GET_SYM(atomic)==FfiAtomicSid)
		return fklNewVMudata(FfiMemUdSid,&FfiAtomicMemMethodTable,fklFfiNewMem(type,size));
	else if(FKL_GET_SYM(atomic)==FfiRawSid)
		return fklNewVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiNewMem(type,size));
	else
		return NULL;
}

FklVMudata* fklFfiNewMemRefUd(FklFfiMem* m,FklVMvalue* selector,FklVMvalue* pindex)
{
	if(selector==NULL||selector==FKL_VM_NIL)
	{
		if(pindex&&(!fklFfiIsPtrTypeId(m->type)&&!fklFfiIsArrayTypeId(m->type)))
			return NULL;
		if(fklFfiIsPtrTypeId(m->type)||fklFfiIsArrayTypeId(m->type))
		{
			int64_t index=fklGetInt(pindex);
			if(fklFfiIsPtrTypeId(m->type))
			{
				FklDefPtrType* pt=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				void* p=*(void**)m->mem;
				return fklNewVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiNewRef(pt->ptype,p+index*fklFfiGetTypeSizeWithTypeId(pt->ptype)));
			}
			else
			{
				FklDefArrayType* at=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				return fklNewVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiNewRef(at->etype,m->mem+index*fklFfiGetTypeSizeWithTypeId(at->etype)));
			}
		}
		else
			return fklNewVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiNewRef(m->type,m->mem));
	}
	else
	{
		FklSid_t selectorId=FKL_GET_SYM(selector);
		if(selectorId==FfiRefSid)
		{
			FklFfiMem* ptr=fklFfiNewMem(fklFfiNewPtrType(m->type),sizeof(void*));
			*(void**)ptr->mem=m->mem;
			return fklNewVMudata(FfiMemUdSid,&FfiAtomicMemMethodTable,ptr);
		}
		else if(selectorId==FfiDeRefSid)
		{
			if(!fklFfiIsPtrTypeId(m->type))
				return NULL;
			FklDefPtrType* ptrType=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
			FklFfiMem* deref=fklFfiNewRef(ptrType->ptype,*(void**)m->mem);
			return fklNewVMudata(FfiMemUdSid,&FfiMemMethodTable,deref);
		}
		else if(fklFfiIsStructTypeId(m->type)||fklFfiIsUnionTypeId(m->type))
		{
			if(fklFfiIsStructTypeId(m->type))
			{
				FklDefStructType* st=FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(m->type).all);
				size_t offset=0;
				for(uint32_t i=0;i<st->num;i++)
				{
					FklDefTypeUnion tu=fklFfiGetTypeUnion(st->layout[i].type);
					size_t align=fklFfiGetTypeAlign(tu);
					offset+=(offset%align)?align-offset%align:0;
					if(st->layout[i].memberSymbol==selectorId)
					{
						return fklNewVMudata(FfiMemUdSid
								,&FfiMemMethodTable
								,fklFfiNewRef(st->layout[i].type,m->mem+offset));
					}
					size_t memberSize=fklFfiGetTypeSize(tu);
					offset+=memberSize;
				}
			}
		}
	}
	return NULL;
}

int fklFfiIsMem(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.ud->type==FfiMemUdSid;
}

int fklFfiSetMem(FklFfiMem* ref,FklVMvalue* pmem)
{
	size_t refSize=fklFfiGetTypeSizeWithTypeId(ref->type);
	size_t memSize=(pmem==FKL_VM_NIL)?0
		:FKL_IS_I32(pmem)?sizeof(int32_t)
		:FKL_IS_I64(pmem)?sizeof(int64_t)
		:fklFfiGetTypeSizeWithTypeId(((FklFfiMem*)pmem->u.ud->mem)->type);
	if(refSize<memSize)
		return 1;
	else if(memSize)
	{
		if(fklIsInt(pmem))
		{
			int64_t i=fklGetInt(pmem);
			memcpy(ref->mem,&i,refSize);
		}
		else
		{
			FklFfiMem* mem=pmem->u.ud->mem;
			memcpy(ref->mem,mem->mem,refSize);
		}
	}
	else
	{
		void* m=NULL;
		memcpy(ref->mem,&m,refSize);
	}
	return 0;
}

int fklFfiIsNull(FklFfiMem* m)
{
	return (fklFfiIsVptrTypeId(m->type)||fklFfiIsPtrTypeId(m->type))&&!(*(void**)m->mem);
}
