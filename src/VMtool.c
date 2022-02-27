#include<fakeLisp/VMtool.h>
#include<fakeLisp/fakeFFI.h>
#include<fakeLisp/common.h>
#include<fakeLisp/opcode.h>
#include"utils.h"
#include<string.h>
#include<stdio.h>
#include<math.h>
#include<pthread.h>
#include<ffi.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

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
#define CAST_TO_I32(TYPE) return MAKE_VM_I32(*(TYPE*)mem);
#define CAST_TO_I64(TYPE) int64_t t=*(TYPE*)mem;return fklNewVMvalue(FKL_I64,&t,heap);
#define ARGL uint8_t* mem,VMheap* heap
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
static FklVMvalue* castChar     (ARGL){return MAKE_VM_CHR(*(char*)mem);}
static FklVMvalue* castWchar_t  (ARGL){return MAKE_VM_I32(*(wchar_t*)mem);}
static FklVMvalue* castFloat    (ARGL){double t=*(float*)mem;return fklNewVMvalue(FKL_DBL,&t,heap);}
static FklVMvalue* castDouble   (ARGL){double t=*(double*)mem;return fklNewVMvalue(FKL_DBL,&t,heap);}
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
static FklVMvalue*(*MemoryCasterList[])(uint8_t*,VMheap*)=
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
#define SET_NUM(TYPE) BODY(!IS_I32(v)&&!IS_I64(v),TYPE,IS_I32(v)?GET_I32(v):v->u.i64)
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
static int setChar     (ARGL){BODY(!IS_CHR(v),char,GET_CHR(v))}
static int setWchar_t  (ARGL){BODY(!IS_CHR(v)&&!IS_I32(v),wchar_t,IS_I32(v)?GET_I32(v):GET_CHR(v))}
static int setFloat    (ARGL){BODY(!IS_DBL(v)&&!IS_I32(v)&&IS_I64(v),float,IS_DBL(v)?v->u.dbl:(IS_I32(v)?GET_I32(v):v->u.i64))}
static int setDouble   (ARGL){BODY(!IS_DBL(v)&&!IS_I32(v)&&IS_I64(v),double,IS_DBL(v)?v->u.dbl:(IS_I32(v)?GET_I32(v):v->u.i64))}
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
FklTypeId_t LastNativeTypeId=0;
FklTypeId_t CharTypeId=0;
FklTypeId_t StringTypeId=0;
FklTypeId_t FILEpTypeId=0;
extern FklSymbolTable GlobSymbolTable;
static int isNativeType(FklSid_t typeName,FklVMDefTypes* otherTypes);
struct GlobTypeUnionListStruct
{
	FklTypeId_t num;
	FklVMTypeUnion* ul;
} GlobTypeUnionList={0,NULL};

static void initVMStructTypeId(FklTypeId_t id,const char* structName,uint32_t num,FklSid_t* symbols,FklTypeId_t* memberTypes)
{
	size_t totalSize=0;
	for(uint32_t i=0;i<num;totalSize+=fklGetVMTypeSize(fklGetVMTypeUnion(memberTypes[i])),i++);
	FklVMTypeUnion* pst=&GlobTypeUnionList.ul[id-1];
	FklVMStructType* ost=(FklVMStructType*)GET_TYPES_PTR(pst->st);
	ost=(FklVMStructType*)realloc(ost,sizeof(FklVMStructType)+num*sizeof(FklVMStructMember));
	FKL_ASSERT(ost,"initVMStructTypeId",__FILE__,__LINE__);
	if(structName)
		ost->type=fklAddSymbolToGlob(structName)->id;
	else
		ost->type=-1;
	ost->num=num;
	ost->totalSize=totalSize;
	for(uint32_t i=0;i<num;i++)
	{
		ost->layout[i].memberSymbol=symbols[i];
		ost->layout[i].type=memberTypes[i];
	}
	pst->st=MAKE_STRUCT_TYPE(ost);
}

static void initVMUnionTypeId(FklTypeId_t id,const char* unionName,uint32_t num,FklSid_t* symbols,FklTypeId_t* memberTypes)
{
	size_t maxSize=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklGetVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
			maxSize=curSize;
	}
	FklVMTypeUnion* put=&GlobTypeUnionList.ul[id-1];
	FklVMUnionType* out=(FklVMUnionType*)GET_TYPES_PTR(put->ut);
	out=(FklVMUnionType*)realloc(out,sizeof(FklVMUnionType)+num*sizeof(FklVMStructMember));
	FKL_ASSERT(out,"initVMUnionTypeId",__FILE__,__LINE__);
	if(unionName)
		out->type=fklAddSymbolToGlob(unionName)->id;
	else
		out->type=-1;
	out->num=num;
	out->maxSize=maxSize;
	for(uint32_t i=0;i<num;i++)
	{
		out->layout[i].memberSymbol=symbols[i];
		out->layout[i].type=memberTypes[i];
	}
	put->ut=MAKE_UNION_TYPE(out);
}

static FklTypeId_t addToGlobTypeUnionList(FklVMTypeUnion type)
{
	GlobTypeUnionList.num+=1;
	size_t num=GlobTypeUnionList.num;
	GlobTypeUnionList.ul=(FklVMTypeUnion*)realloc(GlobTypeUnionList.ul,sizeof(FklVMTypeUnion)*num);
	FKL_ASSERT(GlobTypeUnionList.ul,"addToGlobTypeUnionList",__FILE__,__LINE__);
	GlobTypeUnionList.ul[num-1]=type;
	return num;
}

/*genTypeId functions list*/
static FklTypeId_t genArrayTypeId(FklAstCptr* compositeDataHead,FklVMDefTypes* otherTypes)
{
	FklAstCptr* numCptr=fklNextCptr(compositeDataHead);
	if(!numCptr||numCptr->type!=FKL_ATM||numCptr->u.atom->type!=FKL_I32)
		return 0;
	FklAstCptr* typeCptr=fklNextCptr(numCptr);
	if(!typeCptr)
		return 0;
	FklTypeId_t type=fklGenDefTypesUnion(typeCptr,otherTypes);
	if(!type)
		return type;
	if(fklGetCptrCdr(typeCptr)->type!=FKL_NIL)
		return 0;
	return fklNewVMArrayType(type,numCptr->u.atom->value.i32);
}

static FklTypeId_t genPtrTypeId(FklAstCptr* compositeDataHead,FklVMDefTypes* otherTypes)
{
	FklAstCptr* ptrTypeCptr=fklNextCptr(compositeDataHead);
	if(ptrTypeCptr->type==FKL_ATM&&ptrTypeCptr->u.atom->type!=FKL_SYM)
		return 0;
	FklTypeId_t pType=fklGenDefTypesUnion(ptrTypeCptr,otherTypes);
	if(!pType)
		return pType;
	if(fklGetCptrCdr(ptrTypeCptr)->type!=FKL_NIL)
		return 0;
	return fklNewVMPtrType(pType);
}

static int isInPtrDeclare(FklAstCptr* compositeDataHead)
{
	FklAstPair* outerPair=compositeDataHead->outer;
	FklAstPair* outerPrevPair=outerPair->prev;
	if(outerPrevPair)
	{
		FklAstCptr* outerTypeHead=fklPrevCptr(&outerPrevPair->car);
		if(outerTypeHead)
			return outerTypeHead->type==FKL_ATM&&outerTypeHead->u.atom->type==FKL_SYM&&!strcmp(outerTypeHead->u.atom->value.str,"ptr");
	}
	return 0;
}

static FklTypeId_t genStructTypeId(FklAstCptr* compositeDataHead,FklVMDefTypes* otherTypes)
{
	char* structName=NULL;
	FklAstCptr* structNameCptr=fklNextCptr(compositeDataHead);
	uint32_t num=0;
	FklAstCptr* memberCptr=NULL;
	if(structNameCptr->type==FKL_ATM)
	{
		if(structNameCptr->u.atom->type==FKL_SYM)
			structName=structNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=fklNextCptr(structNameCptr);
		structNameCptr=memberCptr;
	}
	else
		memberCptr=structNameCptr;
	FklTypeId_t retval=0;
	if(memberCptr==NULL&&structName!=NULL)
	{
		FklTypeId_t i=0;
		uint32_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklVMTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(GET_TYPES_TAG(typeUnion.all)==FKL_STRUCT_TYPE_TAG)
			{
				FklVMStructType* st=(FklVMStructType*)GET_TYPES_PTR(typeUnion.st);
				if(st->type==fklAddSymbolToGlob(structName)->id)
				{
					retval=i+1;
					break;
				}
			}
		}
		if(!retval&&!isInPtrDeclare(compositeDataHead))
			return retval;
	}
	else if(structName!=NULL)
		retval=fklNewVMStructType(structName,0,NULL,NULL);
	for(;memberCptr;num++,memberCptr=fklNextCptr(memberCptr));
	memberCptr=structNameCptr;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberCptr,"fklGenDefTypesUnion",__FILE__,__LINE__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=fklNextCptr(memberCptr))
		{
			FklAstCptr* cdr=fklGetCptrCdr(memberCptr);
			if(cdr->type!=FKL_PAIR&&cdr->type!=FKL_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklAstCptr* memberName=fklGetFirstCptr(memberCptr);
			if(memberName->type!=FKL_ATM||memberName->u.atom->type!=FKL_SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklSid_t symbol=fklAddSymbolToGlob(memberName->u.atom->value.str)->id;
			if(fklNextCptr(fklNextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklTypeId_t type=fklGenDefTypesUnion(fklNextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(fklGetCptrCdr(fklNextCptr(memberName))->type!=FKL_NIL)
			{
				free(memberSymbolList);
				free(memberTypeList);
				return 0;
			}
			memberSymbolList[i]=symbol;
			memberTypeList[i]=type;
		}
	}
	if(retval&&num)
	{
		initVMStructTypeId(retval,structName,num,memberSymbolList,memberTypeList);
	}
	else if(!retval)
	{
		retval=fklNewVMStructType(structName,num,memberSymbolList,memberTypeList);
		if(memberTypeList&&memberSymbolList)
		{
			free(memberSymbolList);
			free(memberTypeList);
		}
	}
	return retval;
}

static FklTypeId_t genUnionTypeId(FklAstCptr* compositeDataHead,FklVMDefTypes* otherTypes)
{
	char* unionName=NULL;
	FklAstCptr* unionNameCptr=fklNextCptr(compositeDataHead);
	uint32_t num=0;
	FklAstCptr* memberCptr=NULL;
	if(unionNameCptr->type==FKL_ATM)
	{
		if(unionNameCptr->u.atom->type==FKL_SYM)
			unionName=unionNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=fklNextCptr(unionNameCptr);
		unionNameCptr=memberCptr;
	}
	else
		memberCptr=unionNameCptr;
	FklTypeId_t retval=0;
	if(memberCptr==NULL&&unionName!=NULL)
	{
		FklTypeId_t i=0;
		uint32_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklVMTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(GET_TYPES_TAG(typeUnion.all)==FKL_UNION_TYPE_TAG)
			{
				FklVMUnionType* ut=(FklVMUnionType*)GET_TYPES_PTR(typeUnion.st);
				if(ut->type==fklAddSymbolToGlob(unionName)->id)
				{
					retval=i+1;
					break;
				}
			}
		}
		if(!retval)
			return retval;
	}
	else if(unionName!=NULL)
		retval=fklNewVMUnionType(unionName,0,NULL,NULL);
	for(;memberCptr;num++,memberCptr=fklNextCptr(memberCptr));
	memberCptr=unionNameCptr;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberCptr,"fklGenDefTypesUnion",__FILE__,__LINE__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=fklNextCptr(memberCptr))
		{
			FklAstCptr* cdr=fklGetCptrCdr(memberCptr);
			if(cdr->type!=FKL_PAIR&&cdr->type!=FKL_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklAstCptr* memberName=fklGetFirstCptr(memberCptr);
			if(memberName->type!=FKL_ATM||memberName->u.atom->type!=FKL_SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklSid_t symbol=fklAddSymbolToGlob(memberName->u.atom->value.str)->id;
			if(fklNextCptr(fklNextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklTypeId_t type=fklGenDefTypesUnion(fklNextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(fklGetCptrCdr(fklNextCptr(memberName))->type!=FKL_NIL)
			{
				free(memberSymbolList);
				free(memberTypeList);
				return 0;
			}
			memberSymbolList[i]=symbol;
			memberTypeList[i]=type;
		}
	}
	if(retval&&num)
	{
		initVMUnionTypeId(retval,unionName,num,memberTypeList,memberTypeList);
	}
	else if(!retval)
	{
		retval=fklNewVMUnionType(unionName,num,memberSymbolList,memberTypeList);
		if(memberTypeList&&memberSymbolList)
		{
			free(memberSymbolList);
			free(memberTypeList);
		}
	}
	return retval;
}

static FklTypeId_t genFuncTypeId(FklAstCptr* compositeDataHead,FklVMDefTypes* otherTypes)
{
	FklTypeId_t rtype=0;
	FklAstCptr* argCptr=fklNextCptr(compositeDataHead);
	if(!argCptr||(argCptr->type!=FKL_PAIR&&argCptr->type!=FKL_NIL))
		return 0;
	FklAstCptr* rtypeCptr=fklNextCptr(argCptr);
	if(rtypeCptr)
	{
		FklTypeId_t tmp=fklGenDefTypesUnion(rtypeCptr,otherTypes);
		if(!tmp)
			return 0;
		FklVMTypeUnion tu=fklGetVMTypeUnion(tmp);
		if(GET_TYPES_TAG(tu.all)!=FKL_NATIVE_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FKL_PTR_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FKL_ARRAY_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FKL_FUNC_TYPE_TAG)
			return 0;
		rtype=tmp;
	}
	uint32_t i=0;
	FklAstCptr* firArgCptr=fklGetFirstCptr(argCptr);
	for(;firArgCptr;firArgCptr=fklNextCptr(firArgCptr),i++);
	FklTypeId_t* atypes=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*i);
	FKL_ASSERT(atypes,"fklGenDefTypesUnion",__FILE__,__LINE__);
	for(i=0,firArgCptr=fklGetFirstCptr(argCptr);firArgCptr;firArgCptr=fklNextCptr(firArgCptr),i++)
	{
		FklTypeId_t tmp=fklGenDefTypesUnion(firArgCptr,otherTypes);
		if(!tmp)
		{
			free(atypes);
			return 0;
		}
		FklVMTypeUnion tu=fklGetVMTypeUnion(tmp);
		if(GET_TYPES_TAG(tu.all)!=FKL_NATIVE_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FKL_PTR_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FKL_ARRAY_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FKL_FUNC_TYPE_TAG)
		{
			free(atypes);
			return 0;
		}
		FklAstCptr* cdr=fklGetCptrCdr(firArgCptr);
		if(!tmp||(cdr->type!=FKL_PAIR&&cdr->type!=FKL_NIL))
		{
			free(atypes);
			return 0;
		}
		atypes[i]=tmp;
	}
	FklTypeId_t retval=fklNewVMFuncType(rtype,i,atypes);
	free(atypes);
	return retval;
}

/*------------------------*/

static CRL* newCRL(FklVMpair* pair,int32_t count)
{
	CRL* tmp=(CRL*)malloc(sizeof(CRL));
	FKL_ASSERT(tmp,"newCRL",__FILE__,__LINE__);
	tmp->pair=pair;
	tmp->count=count;
	tmp->next=NULL;
	return tmp;
}

static int32_t findCRLcount(FklVMpair* pair,CRL* h)
{
	for(;h;h=h->next)
	{
		if(h->pair==pair)
			return h->count;
	}
	return -1;
}

static FklVMpair* hasSameVMpair(FklVMpair* begin,FklVMpair* other,CRL* h)
{
	FklVMpair* tmpPair=NULL;
	if(findCRLcount(begin,h)!=-1||findCRLcount(other,h)!=-1)
		return NULL;
	if(begin==other)
		return begin;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->car))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->cdr))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->car))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->cdr))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->car))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->cdr))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->car))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->cdr))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

FklVMpair* isCircularReference(FklVMpair* begin,CRL* h)
{
	FklVMpair* tmpPair=NULL;
	if(IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin,begin->car->u.pair,h);
	if(tmpPair)
		return tmpPair;
	if(IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin,begin->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

int8_t isInTheCircle(FklVMpair* obj,FklVMpair* begin,FklVMpair* curPair)
{
	if(obj==curPair)
		return 1;
	if((IS_PAIR(curPair->car)&&begin==curPair->car->u.pair)||(IS_PAIR(curPair->cdr)&&begin==curPair->cdr->u.pair))
		return 0;
	return ((IS_PAIR(curPair->car))&&isInTheCircle(obj,begin,curPair->car->u.pair))||((IS_PAIR(curPair->cdr))&&isInTheCircle(obj,begin,curPair->cdr->u.pair));
}



pthread_mutex_t FklVMenvGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;

#define INCREASE_REFCOUNT(TYPE,PV) {\
	if((PV))\
	{\
		pthread_mutex_lock(&(TYPE##GlobalRefcountLock));\
		((TYPE*)(PV))->refcount+=1;\
		pthread_mutex_unlock(&(TYPE##GlobalRefcountLock));\
	}\
}

#define DECREASE_REFCOUNT(TYPE,PV) {\
	if((PV))\
	{\
		pthread_mutex_lock(&(TYPE##GlobalRefcountLock));\
		((TYPE*)(PV))->refcount-=1;\
		pthread_mutex_unlock(&(TYPE##GlobalRefcountLock));\
	}\
}

FklVMproc* fklNewVMproc(uint32_t scp,uint32_t cpc)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp,"fklNewVMproc",__FILE__,__LINE__);
	tmp->prevEnv=NULL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	return tmp;
}

FklVMvalue* fklCopyVMvalue(FklVMvalue* obj,VMheap* heap)
{
	FklComStack* s1=fklNewComStack(32);
	FklComStack* s2=fklNewComStack(32);
	FklVMvalue* tmp=VM_NIL;
	fklPushComStack(obj,s1);
	fklPushComStack(&tmp,s2);
	while(!fklIsComStackEmpty(s1))
	{
		FklVMvalue* root=fklPopComStack(s1);
		FklVMptrTag tag=GET_TAG(root);
		FklVMvalue** root1=fklPopComStack(s2);
		switch(tag)
		{
			case FKL_NIL_TAG:
			case FKL_I32_TAG:
			case FKL_SYM_TAG:
			case FKL_CHR_TAG:
				*root1=root;
				break;
			case FKL_PTR_TAG:
				{
					FklValueType type=root->type;
					switch(type)
					{
						case FKL_DBL:
							*root1=fklNewVMvalue(FKL_DBL,&root->u.dbl,heap);
							break;
						case FKL_BYTS:
							*root1=fklNewVMvalue(FKL_BYTS,fklNewVMByts(root->u.byts->size,root->u.byts->str),heap);
							break;
						case FKL_STR:
							*root1=fklNewVMvalue(FKL_STR,fklCopyStr(root->u.str),heap);
							break;
						case FKL_CONT:
						case FKL_PRC:
						case FKL_FP:
						case FKL_DLL:
						case FKL_DLPROC:
						case FKL_ERR:
							*root1=root;
							break;
						case FKL_CHAN:
							{
								FklVMChanl* objCh=root->u.chan;
								FklVMChanl* tmpCh=fklNewVMChanl(objCh->max);
								FklQueueNode* cur=objCh->messages->head;
								for(;cur;cur=cur->next)
								{
									void* tmp=VM_NIL;
									fklPushComQueue(tmp,tmpCh->messages);
									fklPushComStack(cur->data,s1);
									fklPushComStack(tmp,s2);
								}
								*root1=fklNewVMvalue(FKL_CHAN,tmpCh,heap);
							}
							break;
						case FKL_PAIR:
							*root1=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),heap);
							fklPushComStack(&(*root1)->u.pair->car,s2);
							fklPushComStack(&(*root1)->u.pair->cdr,s2);
							fklPushComStack(root->u.pair->car,s1);
							fklPushComStack(root->u.pair->cdr,s1);
							break;
						default:
							break;
					}
					*root1=MAKE_VM_PTR(*root1);
				}
				break;
			default:
				return NULL;
				break;
		}
	}
	fklFreeComStack(s1);
	fklFreeComStack(s2);
	return tmp;
}

FklVMvalue* fklNewVMvalue(FklValueType type,void* pValue,VMheap* heap)
{
	switch((int)type)
	{
		case FKL_NIL:
			return VM_NIL;
			break;
		case FKL_CHR:
			return MAKE_VM_CHR(pValue);
			break;
		case FKL_I32:
			return MAKE_VM_I32(pValue);
			break;
		case FKL_SYM:
			return MAKE_VM_SYM(pValue);
			break;
		default:
			{
				FklVMvalue* tmp=(FklVMvalue*)malloc(sizeof(FklVMvalue));
				FKL_ASSERT(tmp,"fklNewVMvalue",__FILE__,__LINE__);
				tmp->type=type;
				tmp->mark=0;
				tmp->next=heap->head;
				tmp->prev=NULL;
				pthread_mutex_lock(&heap->lock);
				if(heap->head!=NULL)heap->head->prev=tmp;
				heap->head=tmp;
				heap->num+=1;
				pthread_mutex_unlock(&heap->lock);
				switch(type)
				{
					case FKL_DBL:
						if(pValue)
							tmp->u.dbl=*(double*)pValue;
						break;
					case FKL_I64:
						if(pValue)
							tmp->u.i64=*(int64_t*)pValue;
						break;
					case FKL_STR:
						tmp->u.str=pValue;break;
					case FKL_PAIR:
						tmp->u.pair=pValue;break;
					case FKL_PRC:
						tmp->u.prc=pValue;break;
					case FKL_BYTS:
						tmp->u.byts=pValue;break;
					case FKL_CONT:
						tmp->u.cont=pValue;break;
					case FKL_CHAN:
						tmp->u.chan=pValue;break;
					case FKL_FP:
						tmp->u.fp=pValue;break;
					case FKL_DLL:
						tmp->u.dll=pValue;break;
					case FKL_DLPROC:
						tmp->u.dlproc=pValue;break;
					case FKL_FLPROC:
						tmp->u.flproc=pValue;break;
					case FKL_ERR:
						tmp->u.err=pValue;break;
					case FKL_CHF:
						tmp->u.chf=pValue;break;
					case FKL_MEM:
						tmp->u.chf=pValue;break;
					default:
						return NULL;
						break;
				}
				return MAKE_VM_PTR(tmp);
			}
			break;
	}
}

FklVMvalue* fklNewTrueValue(VMheap* heap)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_I32,(FklVMptr)1,heap);
	return tmp;
}

FklVMvalue* fklNewNilValue(VMheap* heap)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_NIL,NULL,heap);
	return tmp;
}

FklVMvalue* fklGetTopValue(FklVMstack* stack)
{
	return stack->values[stack->tp-1];
}

FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place)
{
	return stack->values[place];
}

FklVMvalue* fklGetVMpairCar(FklVMvalue* obj)
{
	return obj->u.pair->car;
}

FklVMvalue* fklGetVMpairCdr(FklVMvalue* obj)
{
	return obj->u.pair->cdr;
}

int fklVMvaluecmp(FklVMvalue* fir,FklVMvalue* sec)
{
	FklComStack* s1=fklNewComStack(32);
	FklComStack* s2=fklNewComStack(32);
	fklPushComStack(fir,s1);
	fklPushComStack(sec,s2);
	int r=1;
	while(!fklIsComStackEmpty(s1))
	{
		FklVMvalue* root1=fklPopComStack(s1);
		FklVMvalue* root2=fklPopComStack(s2);
		if(!IS_PTR(root1)&&!IS_PTR(root2)&&root1!=root2)
			r=0;
		else if(GET_TAG(root1)!=GET_TAG(root2))
			r=0;
		else if(IS_PTR(root1)&&IS_PTR(root2))
		{
			if(root1->type!=root2->type)
				r=0;
			switch(root1->type)
			{
				case FKL_DBL:
					r=(fabs(root1->u.dbl-root2->u.dbl)==0);
					break;
				case FKL_I64:
					r=(root1->u.i64-root2->u.i64)==0;
				case FKL_STR:
					r=!strcmp(root1->u.str,root2->u.str);
					break;
				case FKL_BYTS:
					r=fklEqVMByts(root1->u.byts,root2->u.byts);
					break;
				case FKL_PAIR:
					r=1;
					fklPushComStack(root1->u.pair->car,s1);
					fklPushComStack(root1->u.pair->cdr,s1);
					fklPushComStack(root2->u.pair->car,s2);
					fklPushComStack(root2->u.pair->cdr,s2);
					break;
				default:
					r=(root1==root2);
					break;
			}
		}
		if(!r)
			break;
	}
	fklFreeComStack(s1);
	fklFreeComStack(s2);
	return r;
}

int subfklVMvaluecmp(FklVMvalue* fir,FklVMvalue* sec)
{
	return fir==sec;
}

int fklNumcmp(FklVMvalue* fir,FklVMvalue* sec)
{
	if(GET_TAG(fir)==GET_TAG(sec)&&GET_TAG(fir)==FKL_I32_TAG)
		return fir==sec;
	else
	{
		double first=(GET_TAG(fir)==FKL_I32_TAG)?GET_I32(fir):fir->u.dbl;
		double second=(GET_TAG(sec)==FKL_I32_TAG)?GET_I32(sec):sec->u.dbl;
		return fabs(first-second)==0;
	}
}

FklVMenv* fklNewVMenv(FklVMenv* prev)
{
	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
	FKL_ASSERT(tmp,"fklNewVMenv",__FILE__,__LINE__);
	tmp->num=0;
	tmp->list=NULL;
	tmp->prev=prev;
	fklIncreaseVMenvRefcount(prev);
	tmp->refcount=0;
	return tmp;
}

void fklIncreaseVMenvRefcount(FklVMenv* env)
{
	INCREASE_REFCOUNT(FklVMenv,env);
}

void fklDecreaseVMenvRefcount(FklVMenv* env)
{
	DECREASE_REFCOUNT(FklVMenv,env);
}

FklVMpair* fklNewVMpair(void)
{
	FklVMpair* tmp=(FklVMpair*)malloc(sizeof(FklVMpair));
	FKL_ASSERT(tmp,"fklNewVMpair",__FILE__,__LINE__);
	tmp->car=VM_NIL;
	tmp->cdr=VM_NIL;
	return tmp;
}

FklVMvalue* fklCastCptrVMvalue(FklAstCptr* objCptr,VMheap* heap)
{
	FklComStack* s1=fklNewComStack(32);
	FklComStack* s2=fklNewComStack(32);
	FklVMvalue* tmp=VM_NIL;
	fklPushComStack(objCptr,s1);
	fklPushComStack(&tmp,s2);
	while(!fklIsComStackEmpty(s1))
	{
		FklAstCptr* root=fklPopComStack(s1);
		FklVMvalue** root1=fklPopComStack(s2);
		if(root->type==FKL_ATM)
		{
			FklAstAtom* tmpAtm=root->u.atom;
			switch(tmpAtm->type)
			{
				case FKL_I32:
					*root1=MAKE_VM_I32(tmpAtm->value.i32);
					break;
				case FKL_CHR:
					*root1=MAKE_VM_CHR(tmpAtm->value.chr);
					break;
				case FKL_SYM:
					*root1=MAKE_VM_SYM(fklAddSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case FKL_DBL:
					*root1=fklNewVMvalue(FKL_DBL,&tmpAtm->value.dbl,heap);
					break;
				case FKL_BYTS:
					*root1=fklNewVMvalue(FKL_BYTS,fklNewVMByts(tmpAtm->value.byts.size,tmpAtm->value.byts.str),heap);
					break;
				case FKL_STR:
					*root1=fklNewVMvalue(FKL_STR,fklCopyStr(tmpAtm->value.str),heap);
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==FKL_PAIR)
		{
			FklAstPair* objPair=root->u.pair;
			FklVMpair* tmpPair=fklNewVMpair();
			*root1=fklNewVMvalue(FKL_PAIR,tmpPair,heap);
			fklPushComStack(&objPair->car,s1);
			fklPushComStack(&objPair->cdr,s1);
			fklPushComStack(&tmpPair->car,s2);
			fklPushComStack(&tmpPair->cdr,s2);
			*root1=MAKE_VM_PTR(*root1);
		}
	}
	fklFreeComStack(s1);
	fklFreeComStack(s2);
	return tmp;
}

FklVMByts* fklNewVMByts(size_t size,uint8_t* str)
{
	FklVMByts* tmp=(FklVMByts*)malloc(sizeof(FklVMByts)+size);
	FKL_ASSERT(tmp,"fklNewVMByts",__FILE__,__LINE__);
	tmp->size=size;
	memcpy(tmp->str,str,size);
	return tmp;
}

FklVMByts* fklCopyVMByts(const FklVMByts* obj)
{
	if(obj==NULL)return NULL;
	FklVMByts* tmp=(FklVMByts*)malloc(sizeof(FklVMByts)+obj->size);
	FKL_ASSERT(tmp,"fklCopyVMByts",__FILE__,__LINE__);
	memcpy(tmp->str,obj->str,obj->size);
	tmp->size=obj->size;
	return tmp;
}

void fklVMBytsCat(FklVMByts** fir,const FklVMByts* sec)
{
	size_t firSize=(*fir)->size;
	size_t secSize=sec->size;
	*fir=(FklVMByts*)realloc(*fir,sizeof(FklVMByts)+(firSize+secSize)*sizeof(uint8_t));
	FKL_ASSERT(*fir,"fklVMBytsCat",__FILE__,__LINE__);
	(*fir)->size=firSize+secSize;
	memcpy((*fir)->str+firSize,sec->str,secSize);
}
FklVMByts* fklNewEmptyVMByts()
{
	FklVMByts* tmp=(FklVMByts*)malloc(sizeof(FklVMByts));
	FKL_ASSERT(tmp,"fklNewEmptyVMByts",__FILE__,__LINE__);
	tmp->size=0;
	return tmp;
}

uint8_t* fklCopyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp,"fklCopyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

FklVMproc* fklCopyVMproc(FklVMproc* obj,VMheap* heap)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp,"fklCopyVMproc",__FILE__,__LINE__);
	tmp->scp=obj->scp;
	tmp->cpc=obj->cpc;
	tmp->prevEnv=fklCopyVMenv(obj->prevEnv,heap);
	return tmp;
}

FklVMenv* fklCopyVMenv(FklVMenv* objEnv,VMheap* heap)
{
	FklVMenv* tmp=fklNewVMenv(NULL);
	tmp->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*objEnv->num);
	FKL_ASSERT(tmp->list,"fklCopyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->num;i++)
	{
		FklVMenvNode* node=objEnv->list[i];
		FklVMvalue* v=fklCopyVMvalue(node->value,heap);
		FklVMenvNode* tmp=fklNewVMenvNode(v,node->id);
		objEnv->list[i]=tmp;
	}
	tmp->prev=(objEnv->prev->refcount==0)?fklCopyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void fklFreeVMproc(FklVMproc* proc)
{
	if(proc->prevEnv)
		fklFreeVMenv(proc->prevEnv);
	free(proc);
}

void fklFreeVMenv(FklVMenv* obj)
{
	while(obj!=NULL)
	{
		if(!obj->refcount)
		{
			FklVMenv* prev=obj;
			obj=obj->prev;
			int32_t i=0;
			for(;i<prev->num;i++)
				fklFreeVMenvNode(prev->list[i]);
			free(prev->list);
			free(prev);
		}
		else
		{
			fklDecreaseVMenvRefcount(obj);
			break;
		}
	}
}

void fklReleaseSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_unlock(pGClock);
}

void fklLockSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_rdlock(pGClock);
}

FklVMvalue* fklPopVMstack(FklVMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	FklVMvalue* tmp=fklGetTopValue(stack);
	stack->tp-=1;
	return tmp;
}

FklVMenv* fklCastPreEnvToVMenv(FklPreEnv* pe,FklVMenv* prev,VMheap* heap)
{
	int32_t size=0;
	FklPreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	FklVMenv* tmp=fklNewVMenv(prev);
	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
	{
		FklVMvalue* v=fklCastCptrVMvalue(&tmpDef->obj,heap);
		FklVMenvNode* node=fklNewVMenvNode(v,fklAddSymbolToGlob(tmpDef->symbol)->id);
		fklAddVMenvNode(node,tmp);
	}
	return tmp;
}

VMcontinuation* fklNewVMcontinuation(FklVMstack* stack,FklComStack* rstack,FklComStack* tstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	FKL_ASSERT(tmp,"fklNewVMcontinuation",__FILE__,__LINE__);
	FklVMrunnable* state=(FklVMrunnable*)malloc(sizeof(FklVMrunnable)*size);
	FKL_ASSERT(state,"fklNewVMcontinuation",__FILE__,__LINE__);
	int32_t tbnum=tstack->top;
	FklVMTryBlock* tb=(FklVMTryBlock*)malloc(sizeof(FklVMTryBlock)*tbnum);
	FKL_ASSERT(tb,"fklNewVMcontinuation",__FILE__,__LINE__);
	tmp->stack=fklCopyStack(stack);
	tmp->num=size;
	for(;i<size;i++)
	{
		FklVMrunnable* cur=rstack->data[i];
		state[i].cp=cur->cp;
		fklIncreaseVMenvRefcount(cur->localenv);
		state[i].localenv=cur->localenv;
		state[i].cpc=cur->cpc;
		state[i].scp=cur->scp;
		state[i].mark=cur->mark;
	}
	tmp->tnum=tbnum;
	for(i=0;i<tbnum;i++)
	{
		FklVMTryBlock* cur=tstack->data[i];
		tb[i].sid=cur->sid;
		FklComStack* hstack=cur->hstack;
		int32_t handlerNum=hstack->top;
		FklComStack* curHstack=fklNewComStack(handlerNum);
		int32_t j=0;
		for(;j<handlerNum;j++)
		{
			FklVMerrorHandler* curH=hstack->data[i];
			FklVMerrorHandler* h=fklNewVMerrorHandler(curH->type,curH->proc.scp,curH->proc.cpc);
			fklPushComStack(h,curHstack);
		}
		tb[i].hstack=curHstack;
		tb[i].rtp=cur->rtp;
		tb[i].tp=cur->tp;
	}
	tmp->state=state;
	tmp->tb=tb;
	return tmp;
}

void fklFreeVMcontinuation(VMcontinuation* cont)
{
	int32_t i=0;
	int32_t size=cont->num;
	int32_t tbsize=cont->tnum;
	FklVMstack* stack=cont->stack;
	FklVMrunnable* state=cont->state;
	FklVMTryBlock* tb=cont->tb;
	free(stack->tpst);
	free(stack->values);
	free(stack);
	for(;i<size;i++)
		fklFreeVMenv(state[i].localenv);
	for(i=0;i<tbsize;i++)
	{
		FklComStack* hstack=tb[i].hstack;
		while(!fklIsComStackEmpty(hstack))
		{
			FklVMerrorHandler* h=fklPopComStack(hstack);
			fklFreeVMerrorHandler(h);
		}
		fklFreeComStack(hstack);
	}
	free(tb);
	free(state);
	free(cont);
}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,"fklCopyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values,"fklCopyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FKL_ASSERT(tmp->tpst,"fklCopyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

FklVMenvNode* fklNewVMenvNode(FklVMvalue* value,int32_t id)
{
	FklVMenvNode* tmp=(FklVMenvNode*)malloc(sizeof(FklVMenvNode));
	FKL_ASSERT(tmp,"fklNewVMenvNode",__FILE__,__LINE__);
	tmp->id=id;
	tmp->value=value;
	return tmp;
}

FklVMenvNode* fklAddVMenvNode(FklVMenvNode* node,FklVMenv* env)
{
	if(!env->list)
	{
		env->num=1;
		env->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*1);
		FKL_ASSERT(env->list,"fklAddVMenvNode",__FILE__,__LINE__);
		env->list[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=env->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(env->list[mid]->id>=node->id)
				h=mid-1;
			else
				l=mid+1;
		}
		if(env->list[mid]->id<=node->id)
			mid++;
		env->num+=1;
		int32_t i=env->num-1;
		env->list=(FklVMenvNode**)realloc(env->list,sizeof(FklVMenvNode*)*env->num);
		FKL_ASSERT(env->list,"fklAddVMenvNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		env->list[mid]=node;
	}
	return node;
}

FklVMenvNode* fklFindVMenvNode(FklSid_t id,FklVMenv* env)
{
	if(!env->list)
		return NULL;
	int32_t l=0;
	int32_t h=env->num-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		if(env->list[mid]->id>id)
			h=mid-1;
		else if(env->list[mid]->id<id)
			l=mid+1;
		else
			return env->list[mid];
	}
	return NULL;
}

void fklFreeVMenvNode(FklVMenvNode* node)
{
	free(node);
}


FklVMChanl* fklNewVMChanl(int32_t maxSize)
{
	FklVMChanl* tmp=(FklVMChanl*)malloc(sizeof(FklVMChanl));
	FKL_ASSERT(tmp,"fklNewVMChanl",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->messages=fklNewComQueue();
	tmp->sendq=fklNewComQueue();
	tmp->recvq=fklNewComQueue();
	return tmp;
}

int32_t fklGetNumVMChanl(FklVMChanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=fklLengthComQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

void fklFreeVMChanl(FklVMChanl* ch)
{
	pthread_mutex_destroy(&ch->lock);
	fklFreeComQueue(ch->messages);
	FklQueueNode* head=ch->sendq->head;
	for(;head;head=head->next)
		fklFreeSendT(head->data);
	for(head=ch->recvq->head;head;head=head->next)
		fklFreeRecvT(head->data);
	fklFreeComQueue(ch->sendq);
	fklFreeComQueue(ch->recvq);
	free(ch);
}

FklVMChanl* fklCopyVMChanl(FklVMChanl* ch,VMheap* heap)
{
	FklVMChanl* tmpCh=fklNewVMChanl(ch->max);
	FklQueueNode* cur=ch->messages->head;
	FklComQueue* tmp=fklNewComQueue();
	for(;cur;cur=cur->next)
	{
		void* message=fklCopyVMvalue(cur->data,heap);
		fklPushComQueue(message,tmp);
	}
	return tmpCh;
}

void fklFreeVMfp(FILE* fp)
{
	if(fp!=stdin&&fp!=stdout&&fp!=stderr)
		fclose(fp);
}

FklDllHandle* fklNewVMDll(const char* dllName)
{
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	size_t len=strlen(dllName)+strlen(filetype)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDllName,"fklNewVMDll",__FILE__,__LINE__);
	sprintf(realDllName,"%s%s",dllName,filetype);
#ifdef _WIN32
	char* rpath=_fullpath(NULL,realDllName,0);
#else
	char* rpath=realpath(realDllName,0);
#endif
	if(!rpath)
	{
		perror(dllName);
		free(realDllName);
		return NULL;
	}
#ifdef _WIN32
	FklDllHandle handle=LoadLibrary(rpath);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
		free(rpath);
		free(realDllName);
		return NULL;
	}
#else
	FklDllHandle handle=dlopen(rpath,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		putc('\n',stderr);
		free(rpath);
		free(realDllName);
		return NULL;
	}
#endif
	free(realDllName);
	free(rpath);
	return handle;
}

void fklFreeVMDll(FklDllHandle* dll)
{
	if(dll)
	{
#ifdef _WIN32
		FreeLibrary(dll);
#else
		dlclose(dll);
#endif
	}
}

void* fklGetAddress(const char* funcname,FklDllHandle dlhandle)
{
	void* pfunc=NULL;
#ifdef _WIN32
		pfunc=GetProcAddress(dlhandle,funcname);
#else
		pfunc=dlsym(dlhandle,funcname);
#endif
	return pfunc;
}

FklVMDlproc* fklNewVMDlproc(FklDllFunc address,FklVMvalue* dll)
{
	FklVMDlproc* tmp=(FklVMDlproc*)malloc(sizeof(FklVMDlproc));
	FKL_ASSERT(tmp,"fklNewVMDlproc",__FILE__,__LINE__);
	tmp->func=address;
	tmp->dll=dll;
	return tmp;
}

void fklFreeVMDlproc(FklVMDlproc* dlproc)
{
	free(dlproc);
}

FklVMFlproc* fklNewVMFlproc(FklTypeId_t type,void* func)
{
	FklVMFlproc* tmp=(FklVMFlproc*)malloc(sizeof(FklVMFlproc));
	FKL_ASSERT(tmp,"fklNewVMDlproc",__FILE__,__LINE__);
	tmp->type=type;
	tmp->func=func;
	FklVMFuncType* ft=(FklVMFuncType*)GET_TYPES_PTR(fklGetVMTypeUnion(type).all);
	uint32_t anum=ft->anum;
	FklTypeId_t* atypes=ft->atypes;
	ffi_type** ffiAtypes=(ffi_type**)malloc(sizeof(ffi_type*)*anum);
	FKL_ASSERT(ffiAtypes,"invokeFlproc",__FILE__,__LINE__);
	uint32_t i=0;
	for(;i<anum;i++)
	{
		if(atypes[i]>LastNativeTypeId)
			ffiAtypes[i]=fklGetFfiType(LastNativeTypeId);
		else
			ffiAtypes[i]=fklGetFfiType(atypes[i]);
	}
	FklTypeId_t rtype=ft->rtype;
	ffi_type* ffiRtype=NULL;
	if(rtype>LastNativeTypeId)
		ffiRtype=fklGetFfiType(LastNativeTypeId);
	else
		ffiRtype=fklGetFfiType(rtype);
	fklPrepFFIcif(&tmp->cif,anum,ffiAtypes,ffiRtype);
	tmp->atypes=ffiAtypes;
	return tmp;
}

void fklFreeVMFlproc(FklVMFlproc* t)
{
	free(t->atypes);
	free(t);
}

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t,"fklNewVMerror",__FILE__,__LINE__);
	t->who=fklCopyStr(who);
	t->type=fklAddSymbolToGlob(type)->id;
	t->message=fklCopyStr(message);
	return t;
}

FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t,"fklNewVMerror",__FILE__,__LINE__);
	t->who=fklCopyStr(who);
	t->type=type;
	t->message=fklCopyStr(message);
	return t;
}

void fklFreeVMerror(FklVMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

FklRecvT* fklNewRecvT(FklVM* v)
{
	FklRecvT* tmp=(FklRecvT*)malloc(sizeof(FklRecvT));
	FKL_ASSERT(tmp,"fklNewRecvT",__FILE__,__LINE__);
	tmp->v=v;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeRecvT(FklRecvT* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

FklSendT* fklNewSendT(FklVMvalue* m)
{
	FklSendT* tmp=(FklSendT*)malloc(sizeof(FklSendT));
	FKL_ASSERT(tmp,"fklNewSendT",__FILE__,__LINE__);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeSendT(FklSendT* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

void fklChanlRecv(FklRecvT* r,FklVMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(!fklLengthComQueue(ch->messages))
	{
		fklPushComQueue(r,ch->recvq);
		pthread_cond_wait(&r->cond,&ch->lock);
	}
	SET_RETURN("fklChanlRecv",fklPopComQueue(ch->messages),r->v->stack);
	if(fklLengthComQueue(ch->messages)<ch->max)
	{
		FklSendT* s=fklPopComQueue(ch->sendq);
		if(s)
			pthread_cond_signal(&s->cond);
	}
	fklFreeRecvT(r);
	pthread_mutex_unlock(&ch->lock);
}

void fklChanlSend(FklSendT*s,FklVMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(fklLengthComQueue(ch->recvq))
	{
		FklRecvT* r=fklPopComQueue(ch->recvq);
		if(r)
			pthread_cond_signal(&r->cond);
	}
	if(!ch->max||fklLengthComQueue(ch->messages)<ch->max-1)
		fklPushComQueue(s->m,ch->messages);
	else
	{
		if(fklLengthComQueue(ch->messages)>=ch->max-1)
		{
			fklPushComQueue(s,ch->sendq);
			if(fklLengthComQueue(ch->messages)==ch->max-1)
				fklPushComQueue(s->m,ch->messages);
			pthread_cond_wait(&s->cond,&ch->lock);
		}
	}
	fklFreeSendT(s);
	pthread_mutex_unlock(&ch->lock);
}

FklVMTryBlock* fklNewVMTryBlock(FklSid_t sid,uint32_t tp,long int rtp)
{
	FklVMTryBlock* t=(FklVMTryBlock*)malloc(sizeof(FklVMTryBlock));
	FKL_ASSERT(t,"fklNewVMTryBlock",__FILE__,__LINE__);
	t->sid=sid;
	t->hstack=fklNewComStack(32);
	t->tp=tp;
	t->rtp=rtp;
	return t;
}

void fklFreeVMTryBlock(FklVMTryBlock* b)
{
	FklComStack* hstack=b->hstack;
	while(!fklIsComStackEmpty(hstack))
	{
		FklVMerrorHandler* h=fklPopComStack(hstack);
		fklFreeVMerrorHandler(h);
	}
	fklFreeComStack(b->hstack);
	free(b);
}

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint32_t scp,uint32_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t,"fklNewVMerrorHandler",__FILE__,__LINE__);
	t->type=type;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	return t;
}

void fklFreeVMerrorHandler(FklVMerrorHandler* h)
{
	fklFreeVMenv(h->proc.prevEnv);
	free(h);
}

extern const char*  builtInErrorType[NUM_OF_BUILT_IN_ERROR_TYPE];

int fklRaiseVMerror(FklVMvalue* ev,FklVM* exe)
{
	FklVMerror* err=ev->u.err;
	while(!fklIsComStackEmpty(exe->tstack))
	{
		FklVMTryBlock* tb=fklTopComStack(exe->tstack);
		FklVMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!fklIsComStackEmpty(tb->hstack))
		{
			FklVMerrorHandler* h=fklPopComStack(tb->hstack);
			if(h->type==err->type)
			{
				long int increment=exe->rstack->top-tb->rtp;
				unsigned int i=0;
				for(;i<increment;i++)
				{
					FklVMrunnable* runnable=fklPopComStack(exe->rstack);
					fklFreeVMenv(runnable->localenv);
					free(runnable);
				}
				FklVMrunnable* prevRunnable=fklTopComStack(exe->rstack);
				FklVMrunnable* r=fklNewVMrunnable(&h->proc);
				r->localenv=fklNewVMenv(prevRunnable->localenv);
				FklVMenv* curEnv=r->localenv;
				FklSid_t idOfError=tb->sid;
				FklVMenvNode* errorNode=fklFindVMenvNode(idOfError,curEnv);
				if(!errorNode)
					errorNode=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfError),curEnv);
				errorNode->value=ev;
				fklPushComStack(r,exe->rstack);
				fklFreeVMerrorHandler(h);
				return 1;
			}
			fklFreeVMerrorHandler(h);
		}
		fklFreeVMTryBlock(fklPopComStack(exe->tstack));
	}
	fprintf(stderr,"error of %s :%s",err->who,err->message);
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

FklVMrunnable* fklNewVMrunnable(FklVMproc* code)
{
	FklVMrunnable* tmp=(FklVMrunnable*)malloc(sizeof(FklVMrunnable));
	FKL_ASSERT(tmp,"fklNewVMrunnable",__FILE__,__LINE__);
	if(code)
	{
		tmp->cp=code->scp;
		tmp->scp=code->scp;
		tmp->cpc=code->cpc;
	}
	tmp->mark=0;
	return tmp;
}

char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=GlobSymbolTable.idl[node->fid]->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
	t=fklStrCat(t,"Invalid symbol ");
	t=fklStrCat(t,str);
	t=fklStrCat(t," ");
	t=fklStrCat(t,lineNumber);
	free(lineNumber);
	return t;
}

char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=GlobSymbolTable.idl[node->fid]->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
	switch(type)
	{
		case FKL_WRONGARG:
			t=fklStrCat(t,"Wrong arguement ");
			break;
		case FKL_STACKERROR:
			t=fklStrCat(t,"Stack error ");
			break;
		case FKL_TOOMANYARG:
			t=fklStrCat(t,"Too many arguements ");
			break;
		case FKL_TOOFEWARG:
			t=fklStrCat(t,"Too few arguements ");
			break;
		case FKL_CANTCREATETHREAD:
			t=fklStrCat(t,"Can't create thread ");
			break;
		case FKL_SYMUNDEFINE:
			t=fklStrCat(t,"Symbol ");
			t=fklStrCat(t,GlobSymbolTable.idl[fklGetSymbolIdInByteCode(exe->code+r->cp)]->symbol);
			t=fklStrCat(t," is undefined ");
			break;
		case FKL_INVOKEERROR:
			t=fklStrCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure ");
			break;
		case FKL_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t," ");
			break;
		case FKL_DIVZERROERROR:
			t=fklStrCat(t,"Divided by zero ");
			break;
		case FKL_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDASSIGN:
			t=fklStrCat(t,"Invalid assign ");
			break;
		case FKL_INVALIDACCESS:
			t=fklStrCat(t,"Invalid access ");
			break;
	}
	t=fklStrCat(t,lineNumber);
	free(lineNumber);
	return t;
}

int32_t fklGetSymbolIdInByteCode(const uint8_t* code)
{
	char op=*code;
	switch(op)
	{
		case FKL_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FKL_POP_VAR:
		case FKL_POP_ARG:
		case FKL_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}

int fklResBp(FklVMstack* stack)
{
	if(stack->tp>stack->bp)
		return FKL_TOOMANYARG;
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=GET_I32(prevBp);
	stack->tp-=1;
	return 0;
}

void fklPrincVMvalue(FklVMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	FklVMpair* cirPair=NULL;
	int32_t CRLcount=-1;
	int8_t isInCir=0;
	FklVMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",GET_I32(objValue));
			break;
		case FKL_CHR_TAG:
			putc(GET_CHR(objValue),fp);
			break;
		case FKL_SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case FKL_PTR_TAG:
			{
				switch(objValue->type)
				{
					case FKL_DBL:
						fprintf(fp,"%lf",objValue->u.dbl);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",objValue->u.i64);
						break;
					case FKL_STR:
						fprintf(fp,"%s",objValue->u.str);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMMem* mem=objValue->u.chf;
							FklTypeId_t type=mem->type;
							if(type>0&&type<=LastNativeTypeId)
								PrintMemoryRefFuncList[type-1](mem->mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PRC:
						fprintf(fp,"#<proc>");
						break;
					case FKL_PAIR:
						cirPair=isCircularReference(objValue->u.pair,*h);
						if(cirPair)
							isInCir=isInTheCircle(objValue->u.pair,cirPair,cirPair);
						if(cirPair&&isInCir)
						{
							CRL* crl=newCRL(objValue->u.pair,(*h)?(*h)->count+1:0);
							crl->next=*h;
							*h=crl;
							fprintf(fp,"#%d=(",crl->count);
						}
						else
							putc('(',fp);
						for(;IS_PAIR(objValue);objValue=fklGetVMpairCdr(objValue))
						{
							FklVMvalue* tmpValue=fklGetVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								fklPrincVMvalue(tmpValue,fp,h);
							tmpValue=fklGetVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								fklPrincVMvalue(tmpValue,fp,h);
							}
							else if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case FKL_BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,0);
						break;
					case FKL_CONT:
						fprintf(fp,"#<cont>");
						break;
					case FKL_CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FKL_FP:
						fprintf(fp,"#<fp>");
						break;
					case FKL_DLL:
						fprintf(fp,"<#dll>");
						break;
					case FKL_DLPROC:
						fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"%s",objValue->u.err->message);
						break;
					default:
						fprintf(fp,"Bad value!");break;
				}
			}
		default:
			break;
	}
	if(access)
	{
		head=*h;
		while(head)
		{
			CRL* prev=head;
			head=head->next;
			free(prev);
		}
	}
}

void fklPrin1VMvalue(FklVMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	FklVMpair* cirPair=NULL;
	int8_t isInCir=0;
	int32_t CRLcount=-1;
	FklVMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",GET_I32(objValue));
			break;
		case FKL_CHR_TAG:
			fklPrintRawChar(GET_CHR(objValue),fp);
			break;
		case FKL_SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case FKL_PTR_TAG:
			{
				switch(objValue->type)
				{
					case FKL_DBL:
						fprintf(fp,"%lf",objValue->u.dbl);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",objValue->u.i64);
						break;
					case FKL_STR:
						fklPrintRawString(objValue->u.str,fp);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMMem* mem=objValue->u.chf;
							FklTypeId_t type=mem->type;
							if(type>0&&type<=LastNativeTypeId)
								PrintMemoryRefFuncList[type-1](mem->mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PRC:
						fprintf(fp,"#<proc>");
						break;
					case FKL_PAIR:
						cirPair=isCircularReference(objValue->u.pair,*h);
						if(cirPair)
							isInCir=isInTheCircle(objValue->u.pair,cirPair,cirPair);
						if(cirPair&&isInCir)
						{
							CRL* crl=newCRL(objValue->u.pair,(*h)?(*h)->count+1:0);
							crl->next=*h;
							*h=crl;
							fprintf(fp,"#%d=(",crl->count);
						}
						else
							putc('(',fp);
						for(;IS_PAIR(objValue);objValue=fklGetVMpairCdr(objValue))
						{
							FklVMvalue* tmpValue=fklGetVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								fklPrin1VMvalue(tmpValue,fp,h);
							tmpValue=fklGetVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								fklPrin1VMvalue(tmpValue,fp,h);
							}
							else if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case FKL_BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,1);
						break;
					case FKL_CONT:
						fprintf(fp,"#<cont>");
						break;
					case FKL_CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FKL_FP:
						fprintf(fp,"#<fp>");
						break;
					case FKL_DLL:
						fprintf(fp,"<#dll>");
						break;
					case FKL_DLPROC:
						fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"<#err w:%s t:%s m:%s>",objValue->u.err->who,fklGetGlobSymbolWithId(objValue->u.err->type)->symbol,objValue->u.err->message);
						break;
					default:fprintf(fp,"Bad value!");break;
				}
			}
		default:
			break;
	}
	if(access)
	{
		head=*h;
		while(head)
		{
			CRL* prev=head;
			head=head->next;
			free(prev);
		}
	}
}

int fklEqVMByts(const FklVMByts* fir,const FklVMByts* sec)
{
	if(fir->size!=sec->size)
		return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

FklVMvalue* fklGET_VAL(FklVMvalue* P,VMheap* heap)
{
	if(P)
	{
		if(IS_REF(P))
			return *(FklVMvalue**)(GET_PTR(P));
		else if(IS_CHF(P))
		{
			FklVMvalue* t=NULL;
			FklVMMem* mem=P->u.chf;
			if(mem->type>0&&mem->type<=LastNativeTypeId)
			{
				t=MemoryCasterList[mem->type-1](mem->mem,heap);
			}
			else
			{
				t=P;
			}
			return t;
		}
		return P;
	}
	return P;
}

int fklSET_REF(FklVMvalue* P,FklVMvalue* V)
{
	if(IS_MEM(P)||IS_CHF(P))
	{
		FklVMMem* mem=P->u.chf;
		if(mem->type<=0)
			return 1;
		else if(mem->type>LastNativeTypeId)
		{
			if(!IS_I32(V)&&!IS_I64(V))
				return 1;
			mem->mem=(uint8_t*)(IS_I32(V)?GET_I32(V):V->u.i64);
		}
		else if(MemorySeterList[mem->type-1](mem->mem,V))
			return 1;
		return 0;
	}
	else if(IS_REF(P))
	{
		*(FklVMvalue**)GET_PTR(P)=V;
		return 0;
	}
	else
		return 1;
}

FklTypeId_t fklNewVMNativeType(FklSid_t type,size_t size,size_t align)
{
	FklVMNativeType* tmp=(FklVMNativeType*)malloc(sizeof(FklVMNativeType));
	FKL_ASSERT(tmp,"fklNewVMNativeType",__FILE__,__LINE__);
	tmp->type=type;
	tmp->align=align;
	tmp->size=size;
	return addToGlobTypeUnionList((FklVMTypeUnion)MAKE_NATIVE_TYPE(tmp));
}

void fklFreeVMNativeType(FklVMNativeType* obj)
{
	free(obj);
}

FklVMTypeUnion fklGetVMTypeUnion(FklTypeId_t t)
{
	return GlobTypeUnionList.ul[t-1];
}

size_t fklGetVMTypeSizeWithTypeId(FklTypeId_t t)
{
	return fklGetVMTypeSize(fklGetVMTypeUnion(t));
}

size_t fklGetVMTypeSize(FklVMTypeUnion t)
{
	FklDefTypeTag tag=GET_TYPES_TAG(t.all);
	t.all=GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_NATIVE_TYPE_TAG:
			return t.nt->size;
			break;
		case FKL_ARRAY_TYPE_TAG:
			return t.at->totalSize;
			break;
		case FKL_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_STRUCT_TYPE_TAG:
			return t.st->totalSize;
			break;
		case FKL_UNION_TYPE_TAG:
			return t.ut->maxSize;
			break;
		case FKL_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

size_t fklGetVMTypeAlign(FklVMTypeUnion t)
{
	FklDefTypeTag tag=GET_TYPES_TAG(t.all);
	t.all=GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_NATIVE_TYPE_TAG:
			return t.nt->align;
			break;
		case FKL_ARRAY_TYPE_TAG:
			return t.at->align;
			break;
		case FKL_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_STRUCT_TYPE_TAG:
			return t.st->align;
			break;
		case FKL_UNION_TYPE_TAG:
			return t.ut->align;
			break;
		case FKL_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

FklTypeId_t fklNewVMArrayType(FklTypeId_t type,size_t num)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklVMTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(GET_TYPES_TAG(tmpType.all)==FKL_ARRAY_TYPE_TAG)
		{
			FklVMArrayType* arrayType=(FklVMTypeUnion){.at=GET_TYPES_PTR(tmpType.all)}.at;
			if(arrayType->etype==type&&arrayType->num==num)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklVMArrayType* tmp=(FklVMArrayType*)malloc(sizeof(FklVMArrayType));
		FKL_ASSERT(tmp,"fklNewVMArrayType",__FILE__,__LINE__);
		tmp->etype=type;
		tmp->num=num;
		tmp->totalSize=num*fklGetVMTypeSize(fklGetVMTypeUnion(type));
		tmp->align=fklGetVMTypeAlign(fklGetVMTypeUnion(type));
		return addToGlobTypeUnionList((FklVMTypeUnion)MAKE_ARRAY_TYPE(tmp));
	}
	else
		return id;
}

void fklFreeVMArrayType(FklVMArrayType* obj)
{
	free(GET_TYPES_PTR(obj));
}

FklTypeId_t fklNewVMPtrType(FklTypeId_t type)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklVMTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(GET_TYPES_TAG(tmpType.all)==FKL_PTR_TYPE_TAG)
		{
			FklVMPtrType* ptrType=(FklVMTypeUnion){.pt=GET_TYPES_PTR(tmpType.all)}.pt;
			if(ptrType->ptype==type)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklVMPtrType* tmp=(FklVMPtrType*)malloc(sizeof(FklVMPtrType));
		FKL_ASSERT(tmp,"fklNewVMPtrType",__FILE__,__LINE__);
		tmp->ptype=type;
		return addToGlobTypeUnionList((FklVMTypeUnion)MAKE_PTR_TYPE(tmp));
	}
	return id;
}

void fklFreeVMPtrType(FklVMPtrType* obj)
{
	free(GET_TYPES_PTR(obj));
}

FklTypeId_t fklNewVMStructType(const char* structName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[])
{
	FklTypeId_t id=0;
	if(structName)
	{
		size_t i=0;
		size_t num=GlobTypeUnionList.num;
		FklSid_t stype=fklAddSymbolToGlob(structName)->id;
		for(;i<num;i++)
		{
			FklVMTypeUnion tmpType=GlobTypeUnionList.ul[i];
			if(GET_TYPES_TAG(tmpType.all)==FKL_STRUCT_TYPE_TAG)
			{
				FklVMStructType* structType=(FklVMTypeUnion){.st=GET_TYPES_PTR(tmpType.all)}.st;
				if(structType->type==stype)
				{
					id=i+1;
					break;
				}
			}
		}
	}
	if(!id)
	{
		size_t totalSize=0;
		size_t maxSize=0;
		size_t structAlign=0;
		for(uint32_t i=0;i<num;i++)
		{
			FklVMTypeUnion tu=fklGetVMTypeUnion(memberTypes[i]);
			size_t align=fklGetVMTypeAlign(tu);
			size_t size=fklGetVMTypeSize(tu);
			totalSize+=(totalSize%align)?align-totalSize%align:0;
			totalSize+=size;
			if(maxSize<size)
			{
				maxSize=size;
				structAlign=align;
			}
		}
		totalSize+=(totalSize%maxSize)?maxSize-totalSize%maxSize:0;
		FklVMStructType* tmp=(FklVMStructType*)malloc(sizeof(FklVMStructType)+sizeof(FklVMStructMember)*num);
		FKL_ASSERT(tmp,"fklNewVMStructType",__FILE__,__LINE__);
		if(structName)
			tmp->type=fklAddSymbolToGlob(structName)->id;
		else
			tmp->type=-1;
		tmp->num=num;
		tmp->totalSize=totalSize;
		tmp->align=structAlign;
		for(uint32_t i=0;i<num;i++)
		{
			tmp->layout[i].memberSymbol=symbols[i];
			tmp->layout[i].type=memberTypes[i];
		}
		return addToGlobTypeUnionList((FklVMTypeUnion)MAKE_STRUCT_TYPE(tmp));
	}
	return id;
}

FklTypeId_t fklNewVMUnionType(const char* unionName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[])
{
	size_t maxSize=0;
	size_t align=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklGetVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
		{
			maxSize=curSize;
			align=fklGetVMTypeAlign(fklGetVMTypeUnion(memberTypes[i]));
		}
	}
	FklVMUnionType* tmp=(FklVMUnionType*)malloc(sizeof(FklVMUnionType)+sizeof(FklVMStructMember)*num);
	FKL_ASSERT(tmp,"fklNewVMStructType",__FILE__,__LINE__);
	if(unionName)
		tmp->type=fklAddSymbolToGlob(unionName)->id;
	else
		tmp->type=-1;
	tmp->num=num;
	tmp->maxSize=maxSize;
	tmp->align=align;
	for(uint32_t i=0;i<num;i++)
	{
		tmp->layout[i].memberSymbol=symbols[i];
		tmp->layout[i].type=memberTypes[i];
	}
	return addToGlobTypeUnionList((FklVMTypeUnion)MAKE_UNION_TYPE(tmp));
}

void fklFreeVMUnionType(FklVMUnionType* obj)
{
	free(GET_TYPES_PTR(obj));
}

void fklFreeVMStructType(FklVMStructType* obj)
{
	free(GET_TYPES_PTR(obj));
}

FklTypeId_t fklNewVMFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[])
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklVMTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(GET_TYPES_TAG(tmpType.all)==FKL_FUNC_TYPE_TAG)
		{
			FklVMFuncType* funcType=(FklVMTypeUnion){.ft=GET_TYPES_PTR(tmpType.all)}.ft;
			if(funcType->rtype==rtype&&funcType->anum==anum&&!memcmp(funcType->atypes,atypes,sizeof(FklTypeId_t)*anum))
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklVMFuncType* tmp=(FklVMFuncType*)malloc(sizeof(FklVMFuncType)+sizeof(FklTypeId_t)*anum);
		FKL_ASSERT(tmp,"fklNewVMFuncType",__FILE__,__LINE__);
		tmp->rtype=rtype;
		tmp->anum=anum;
		uint32_t i=0;
		for(;i<anum;i++)
			tmp->atypes[i]=atypes[i];
		return addToGlobTypeUnionList((FklVMTypeUnion)MAKE_FUNC_TYPE(tmp));
	}
	return id;
}

void fklFreeVMFuncType(FklVMFuncType* obj)
{
	free(GET_TYPES_PTR(obj));
}

int fklAddDefTypes(FklVMDefTypes* otherTypes,FklSid_t typeName,FklTypeId_t type)
{
	if(otherTypes->num==0)
	{
		otherTypes->num+=1;
		FklVMDefTypesNode* node=(FklVMDefTypesNode*)malloc(sizeof(FklVMDefTypesNode));
		otherTypes->u=(FklVMDefTypesNode**)malloc(sizeof(FklVMDefTypesNode*)*1);
		FKL_ASSERT(otherTypes->u&&node,"fklAddDefTypes",__FILE__,__LINE__);
		node->name=typeName;
		node->type=type;
		otherTypes->u[0]=node;
	}
	else
	{
		int64_t l=0;
		int64_t h=otherTypes->num-1;
		int64_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int64_t r=(int64_t)otherTypes->u[mid]->name-(int64_t)typeName;
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
				return 1;
		}
		if((int64_t)otherTypes->u[mid]->name-(int64_t)typeName<0)
			mid++;
		otherTypes->num+=1;
		int64_t i=otherTypes->num-1;
		otherTypes->u=(FklVMDefTypesNode**)realloc(otherTypes->u,sizeof(FklVMDefTypesNode*)*otherTypes->num);
		FKL_ASSERT(otherTypes->u,"fklAddDefTypes",__FILE__,__LINE__);
		FklVMDefTypesNode* node=(FklVMDefTypesNode*)malloc(sizeof(FklVMDefTypesNode));
		FKL_ASSERT(otherTypes->u&&node,"fklAddDefTypes",__FILE__,__LINE__);
		node->name=typeName;
		node->type=type;
		for(;i>mid;i--)
			otherTypes->u[i]=otherTypes->u[i-1];
		otherTypes->u[mid]=node;
	}
	return 0;
}

FklVMDefTypesNode* fklFindVMDefTypesNode(FklSid_t typeName,FklVMDefTypes* otherTypes)
{
	int64_t l=0;
	int64_t h=otherTypes->num-1;
	int64_t mid=0;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int64_t r=(int64_t)otherTypes->u[mid]->name-(int64_t)typeName;
		if(r>0)
			h=mid-1;
		else if(r<0)
			l=mid+1;
		else
			return otherTypes->u[mid];
	}
	return NULL;
	//return (FklVMTypeUnion){.all=NULL};
}

FklTypeId_t fklGenDefTypes(FklAstCptr* objCptr,FklVMDefTypes* otherTypes,FklSid_t* typeName)
{
	FklAstCptr* fir=fklNextCptr(fklGetFirstCptr(objCptr));
	if(fir->type!=FKL_ATM||fir->u.atom->type!=FKL_SYM)
		return 0;
	FklSid_t typeId=fklAddSymbolToGlob(fir->u.atom->value.str)->id;
	if(isNativeType(typeId,otherTypes))
		return 0;
	*typeName=typeId;
	fir=fklNextCptr(fir);
	if(fir->type!=FKL_ATM&&fir->type!=FKL_PAIR)
		return 0;
	return fklGenDefTypesUnion(fir,otherTypes);
}

FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklVMDefTypes* otherTypes)
{
	if(!otherTypes)
		return 0;
	if(objCptr->type==FKL_ATM&&objCptr->u.atom->type==FKL_SYM)
	{
		FklVMDefTypesNode* n=fklFindVMDefTypesNode(fklAddSymbolToGlob(objCptr->u.atom->value.str)->id,otherTypes);
		if(!n)
			return 0;
		else
			return n->type;
	}
	else if(objCptr->type==FKL_PAIR)
	{
		FklAstCptr* compositeDataHead=fklGetFirstCptr(objCptr);
		if(compositeDataHead->type!=FKL_ATM||compositeDataHead->u.atom->type!=FKL_SYM)
			return 0;
		if(!strcmp(compositeDataHead->u.atom->value.str,"array"))
			return genArrayTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"ptr"))
			return genPtrTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"struct"))
			return genStructTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"union"))
			return genUnionTypeId(compositeDataHead,otherTypes);
		else if(!strcmp(compositeDataHead->u.atom->value.str,"function"))
			return genFuncTypeId(compositeDataHead,otherTypes);
		else
			return 0;
	}
	else
		return 0;
}

int isNativeType(FklSid_t typeName,FklVMDefTypes* otherTypes)
{
	FklVMDefTypesNode* typeNode=fklFindVMDefTypesNode(typeName,otherTypes);
	return typeNode!=NULL&&GET_TYPES_TAG(fklGetVMTypeUnion(typeNode->type).all)==FKL_NATIVE_TYPE_TAG;
}

FklVMMem* fklNewVMMem(FklTypeId_t type,uint8_t* mem)
{
	FklVMMem* tmp=(FklVMMem*)malloc(sizeof(FklVMMem));
	FKL_ASSERT(tmp,"fklNewVMMem",__FILE__,__LINE__);
	tmp->type=type;
	tmp->mem=mem;
	return tmp;
}

void fklInitNativeDefTypes(FklVMDefTypes* otherTypes)
{
	struct
	{
		char* typeName;
		size_t size;
	} nativeTypeList[]=
	{
		{"short",sizeof(short)},
		{"int",sizeof(int)},
		{"unsigned-short",sizeof(unsigned short)},
		{"unsigned",sizeof(unsigned)},
		{"long",sizeof(long)},
		{"unsigned-long",sizeof(unsigned long)},
		{"long-long",sizeof(long long)},
		{"unsigned-long-long",sizeof(unsigned long long)},
		{"ptrdiff",sizeof(ptrdiff_t)},
		{"size_t",sizeof(size_t)},
		{"ssize_t",sizeof(ssize_t)},
		{"char",sizeof(char)},
		{"wchar_t",sizeof(wchar_t)},
		{"float",sizeof(float)},
		{"double",sizeof(double)},
		{"int8_t",sizeof(int8_t)},
		{"uint8_t",sizeof(uint8_t)},
		{"int16_t",sizeof(int16_t)},
		{"uint16_t",sizeof(uint16_t)},
		{"int32_t",sizeof(int32_t)},
		{"uint32_t",sizeof(uint32_t)},
		{"int64_t",sizeof(int64_t)},
		{"uint64_t",sizeof(uint64_t)},
		{"iptr",sizeof(intptr_t)},
		{"uptr",sizeof(uintptr_t)},
		{"vptr",sizeof(void*)},
	};
	size_t num=sizeof(nativeTypeList)/(sizeof(char*)+sizeof(size_t));
	size_t i=0;
	for(;i<num;i++)
	{
		FklSid_t typeName=fklAddSymbolToGlob(nativeTypeList[i].typeName)->id;
		size_t size=nativeTypeList[i].size;
		FklTypeId_t t=fklNewVMNativeType(typeName,size,size);
		if(!CharTypeId&&!strcmp("char",nativeTypeList[i].typeName))
			CharTypeId=t;
		//FklVMTypeUnion t={.nt=fklNewVMNativeType(typeName,size)};
		fklAddDefTypes(otherTypes,typeName,t);
	}
	FklSid_t otherTypeName=fklAddSymbolToGlob("string")->id;
	StringTypeId=fklNewVMNativeType(otherTypeName,sizeof(char*),sizeof(char*));
	fklAddDefTypes(otherTypes,otherTypeName,StringTypeId);
	otherTypeName=fklAddSymbolToGlob("FILE*")->id;
	FILEpTypeId=fklNewVMNativeType(otherTypeName,sizeof(FILE*),sizeof(FILE*));
	fklAddDefTypes(otherTypes,otherTypeName,FILEpTypeId);
	LastNativeTypeId=num;
	//i=0;
	//for(;i<num;i++)
	//	fprintf(stderr,"i=%ld typeId=%d\n",i,otherTypes->u[i]->name);
	//fklPrintGlobSymbolTable(stderr);
}

void fklWriteTypeList(FILE* fp)
{
	FklTypeId_t i=0;
	FklTypeId_t num=GlobTypeUnionList.num;
	FklVMTypeUnion* ul=GlobTypeUnionList.ul;
	fwrite(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fwrite(&CharTypeId,sizeof(CharTypeId),1,fp);
	fwrite(&num,sizeof(num),1,fp);
	for(;i<num;i++)
	{
		FklVMTypeUnion tu=ul[i];
		FklDefTypeTag tag=GET_TYPES_TAG(tu.all);
		void* p=(void*)GET_TYPES_PTR(tu.all);
		fwrite(&tag,sizeof(uint8_t),1,fp);
		switch(tag)
		{
			case FKL_NATIVE_TYPE_TAG:
				fwrite(&((FklVMNativeType*)p)->type,sizeof(((FklVMNativeType*)p)->type),1,fp);
				fwrite(&((FklVMNativeType*)p)->size,sizeof(((FklVMNativeType*)p)->size),1,fp);
				break;
			case FKL_ARRAY_TYPE_TAG:
				fwrite(&((FklVMArrayType*)p)->etype,sizeof(((FklVMArrayType*)p)->etype),1,fp);
				fwrite(&((FklVMArrayType*)p)->num,sizeof(((FklVMArrayType*)p)->num),1,fp);
				break;
			case FKL_PTR_TYPE_TAG:
				fwrite(&((FklVMPtrType*)p)->ptype,sizeof(((FklVMPtrType*)p)->ptype),1,fp);
				break;
			case FKL_STRUCT_TYPE_TAG:
				{
					uint32_t num=((FklVMStructType*)p)->num;
					fwrite(&num,sizeof(num),1,fp);
					fwrite(&((FklVMStructType*)p)->totalSize,sizeof(((FklVMStructType*)p)->totalSize),1,fp);
					FklVMStructMember* members=((FklVMStructType*)p)->layout;
					fwrite(members,sizeof(FklVMStructMember),num,fp);
				}
				break;
			case FKL_UNION_TYPE_TAG:
				{
					uint32_t num=((FklVMUnionType*)p)->num;
					fwrite(&num,sizeof(num),1,fp);
					fwrite(&((FklVMUnionType*)p)->maxSize,sizeof(((FklVMUnionType*)p)->maxSize),1,fp);
					FklVMStructMember* members=((FklVMUnionType*)p)->layout;
					fwrite(members,sizeof(FklVMStructMember),num,fp);
				}
				break;
			case FKL_FUNC_TYPE_TAG:
				{
					FklVMFuncType* ft=(FklVMFuncType*)p;
					fwrite(&ft->rtype,sizeof(ft->rtype),1,fp);
					uint32_t anum=ft->anum;
					fwrite(&anum,sizeof(anum),1,fp);
					FklTypeId_t* atypes=ft->atypes;
					fwrite(atypes,sizeof(*atypes),anum,fp);
				}
				break;
			default:
				break;
		}
	}
}

void fklLoadTypeList(FILE* fp)
{
	FklTypeId_t i=0;
	FklTypeId_t num=0;
	FklVMTypeUnion* ul=NULL;
	fread(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fread(&CharTypeId,sizeof(CharTypeId),1,fp);
	fread(&num,sizeof(num),1,fp);
	ul=(FklVMTypeUnion*)malloc(sizeof(FklVMTypeUnion)*num);
	FKL_ASSERT(ul,"fklLoadTypeList",__FILE__,__LINE__);
	for(;i<num;i++)
	{
		FklDefTypeTag tag=FKL_NATIVE_TYPE_TAG;
		fread(&tag,sizeof(uint8_t),1,fp);
		FklVMTypeUnion tu={.all=NULL};
		switch(tag)
		{
			case FKL_NATIVE_TYPE_TAG:
				{
					FklVMNativeType* t=(FklVMNativeType*)malloc(sizeof(FklVMNativeType));
					FKL_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					fread(&t->type,sizeof(t->type),1,fp);
					fread(&t->size,sizeof(t->size),1,fp);
					tu.nt=(FklVMNativeType*)MAKE_NATIVE_TYPE(t);
				}
				break;
			case FKL_ARRAY_TYPE_TAG:
				{
					FklVMArrayType* t=(FklVMArrayType*)malloc(sizeof(FklVMArrayType));
					FKL_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					fread(&t->etype,sizeof(t->etype),1,fp);
					fread(&t->num,sizeof(t->num),1,fp);
					tu.at=(FklVMArrayType*)MAKE_ARRAY_TYPE(t);
				}
				break;
			case FKL_PTR_TYPE_TAG:
				{
					FklVMPtrType* t=(FklVMPtrType*)malloc(sizeof(FklVMPtrType));
					FKL_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					fread(&t->ptype,sizeof(t->ptype),1,fp);
					tu.pt=(FklVMPtrType*)MAKE_PTR_TYPE(t);
				}
				break;
			case FKL_STRUCT_TYPE_TAG:
				{
					uint32_t num=0;
					fread(&num,sizeof(num),1,fp);
					FklVMStructType* t=(FklVMStructType*)malloc(sizeof(FklVMStructType)+sizeof(FklVMStructMember)*num);
					FKL_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					t->num=num;
					fread(&t->totalSize,sizeof(t->totalSize),1,fp);
					fread(t->layout,sizeof(FklVMStructMember),num,fp);
					tu.st=(FklVMStructType*)MAKE_STRUCT_TYPE(t);
				}
				break;
			case FKL_UNION_TYPE_TAG:
				{
					uint32_t num=0;
					fread(&num,sizeof(num),1,fp);
					FklVMUnionType* t=(FklVMUnionType*)malloc(sizeof(FklVMUnionType)+sizeof(FklVMStructMember)*num);
					FKL_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					t->num=num;
					fread(&t->maxSize,sizeof(t->maxSize),1,fp);
					fread(t->layout,sizeof(FklVMStructMember),num,fp);
					tu.ut=(FklVMUnionType*)MAKE_UNION_TYPE(t);
				}
			case FKL_FUNC_TYPE_TAG:
				{
					FklTypeId_t rtype=0;
					fread(&rtype,sizeof(rtype),1,fp);
					uint32_t anum=0;
					fread(&anum,sizeof(anum),1,fp);
					FklVMFuncType* t=(FklVMFuncType*)malloc(sizeof(FklVMUnionType)+sizeof(FklTypeId_t)*anum);
					FKL_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					t->rtype=rtype;
					t->anum=anum;
					fread(t->atypes,sizeof(FklTypeId_t),anum,fp);
					tu.ft=t;
				}
				break;
			default:
				break;
		}
		ul[i]=tu;
	}
	GlobTypeUnionList.num=num;
	GlobTypeUnionList.ul=ul;
}

void fklFreeGlobTypeList()
{
	FklTypeId_t num=GlobTypeUnionList.num;
	FklVMTypeUnion* ul=GlobTypeUnionList.ul;
	FklTypeId_t i=0;
	for(;i<num;i++)
	{
		FklVMTypeUnion tu=ul[i];
		FklDefTypeTag tag=GET_TYPES_TAG(tu.all);
		switch(tag)
		{
			case FKL_NATIVE_TYPE_TAG:
				fklFreeVMNativeType((FklVMNativeType*)GET_TYPES_PTR(tu.all));
				break;
			case FKL_PTR_TYPE_TAG:
				fklFreeVMPtrType((FklVMPtrType*)GET_TYPES_PTR(tu.all));
				break;
			case FKL_ARRAY_TYPE_TAG:
				fklFreeVMArrayType((FklVMArrayType*)GET_TYPES_PTR(tu.all));
				break;
			case FKL_STRUCT_TYPE_TAG:
				fklFreeVMStructType((FklVMStructType*)GET_TYPES_PTR(tu.all));
				break;
			case FKL_UNION_TYPE_TAG:
				fklFreeVMUnionType((FklVMUnionType*)GET_TYPES_PTR(tu.all));
				break;
			case FKL_FUNC_TYPE_TAG:
				fklFreeVMFuncType((FklVMFuncType*)GET_TYPES_PTR(tu.all));
				break;
			default:
				break;
		}
	}
	free(ul);
}

int fklIsNativeTypeId(FklTypeId_t type)
{
	FklVMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FKL_NATIVE_TYPE_TAG;
}

int fklIsArrayTypeId(FklTypeId_t type)
{
	FklVMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FKL_ARRAY_TYPE_TAG;
}

int fklIsPtrTypeId(FklTypeId_t type)
{
	FklVMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FKL_PTR_TYPE_TAG;
}

int fklIsStructTypeId(FklTypeId_t type)
{
	FklVMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FKL_STRUCT_TYPE_TAG;
}

int fklIsUnionTypeId(FklTypeId_t type)
{
	FklVMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FKL_UNION_TYPE_TAG;
}

int fklIsFunctionTypeId(FklTypeId_t type)
{
	FklVMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FKL_FUNC_TYPE_TAG;
}
