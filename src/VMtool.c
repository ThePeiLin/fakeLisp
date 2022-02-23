#include<fakeLisp/VMtool.h>
#include<fakeLisp/common.h>
#include<fakeLisp/opcode.h>
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
#define PRINT_MEM_REF(FMT,TYPE,MEM,FP) fprintf(FP,FMT,*(TYPE*)MEM)
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
#define CAST_TO_IN32(TYPE) return MAKE_VM_IN32(*(TYPE*)mem);
#define CAST_TO_IN64(TYPE) int64_t t=*(TYPE*)mem;return fklNewVMvalue(IN64,&t,heap);
#define ARGL uint8_t* mem,VMheap* heap
static VMvalue* castShortMem (ARGL){CAST_TO_IN32(short)}
static VMvalue* castIntMem   (ARGL){CAST_TO_IN32(int)}
static VMvalue* castUShortMem(ARGL){CAST_TO_IN32(unsigned short)}
static VMvalue* castUintMem  (ARGL){CAST_TO_IN64(unsigned int)}
static VMvalue* castLongMem  (ARGL){CAST_TO_IN64(long)}
static VMvalue* castULongMem (ARGL){CAST_TO_IN64(unsigned long)}
static VMvalue* castLLongMem (ARGL){CAST_TO_IN64(long long)}
static VMvalue* castULLongMem(ARGL){CAST_TO_IN64(unsigned long long)}
static VMvalue* castPtrdiff_t(ARGL){CAST_TO_IN64(ptrdiff_t)}
static VMvalue* castSize_t   (ARGL){CAST_TO_IN64(size_t)}
static VMvalue* castSsize_t  (ARGL){CAST_TO_IN64(ssize_t)}
static VMvalue* castChar     (ARGL){return MAKE_VM_CHR(*(char*)mem);}
static VMvalue* castWchar_t  (ARGL){return MAKE_VM_IN32(*(wchar_t*)mem);}
static VMvalue* castFloat    (ARGL){double t=*(float*)mem;return fklNewVMvalue(DBL,&t,heap);}
static VMvalue* castDouble   (ARGL){double t=*(double*)mem;return fklNewVMvalue(DBL,&t,heap);}
static VMvalue* castInt8_t   (ARGL){CAST_TO_IN32(int8_t)}
static VMvalue* castUint8_t  (ARGL){CAST_TO_IN32(uint8_t)}
static VMvalue* castInt16_t  (ARGL){CAST_TO_IN32(int16_t)}
static VMvalue* castUint16_t (ARGL){CAST_TO_IN32(uint16_t)}
static VMvalue* castInt32_t  (ARGL){CAST_TO_IN32(int32_t)}
static VMvalue* castUint32_t (ARGL){CAST_TO_IN32(uint32_t)}
static VMvalue* castInt64_t  (ARGL){CAST_TO_IN64(int64_t)}
static VMvalue* castUint64_t (ARGL){CAST_TO_IN64(uint64_t)}
static VMvalue* castIptr     (ARGL){CAST_TO_IN64(intptr_t)}
static VMvalue* castUptr     (ARGL){CAST_TO_IN64(uintptr_t)}
static VMvalue* castVPtr     (ARGL){return fklNewVMvalue(IN64,mem,heap);}
#undef ARGL
#undef CAST_TO_IN32
#undef CAST_TO_IN64
static VMvalue*(*MemoryCasterList[])(uint8_t*,VMheap*)=
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
#define SET_NUM(TYPE) BODY(!IS_IN32(v)&&!IS_IN64(v),TYPE,IS_IN32(v)?GET_IN32(v):*v->u.in64)
#define ARGL uint8_t* mem,VMvalue* v
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
static int setWchar_t  (ARGL){BODY(!IS_CHR(v)&&!IS_IN32(v),wchar_t,IS_IN32(v)?GET_IN32(v):GET_CHR(v))}
static int setFloat    (ARGL){BODY(!IS_DBL(v)&&!IS_IN32(v)&&IS_IN64(v),float,IS_DBL(v)?*v->u.dbl:(IS_IN32(v)?GET_IN32(v):*v->u.in64))}
static int setDouble   (ARGL){BODY(!IS_DBL(v)&&!IS_IN32(v)&&IS_IN64(v),double,IS_DBL(v)?*v->u.dbl:(IS_IN32(v)?GET_IN32(v):*v->u.in64))}
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
TypeId_t LastNativeTypeId=0;
TypeId_t CharTypeId=0;
TypeId_t StringTypeId=0;
TypeId_t FILEpTypeId=0;
extern SymbolTable GlobSymbolTable;
static int isNativeType(Sid_t typeName,VMDefTypes* otherTypes);
struct GlobTypeUnionListStruct
{
	TypeId_t num;
	VMTypeUnion* ul;
} GlobTypeUnionList={0,NULL};

static void initVMStructTypeId(TypeId_t id,const char* structName,uint32_t num,Sid_t* symbols,TypeId_t* memberTypes)
{
	size_t totalSize=0;
	for(uint32_t i=0;i<num;totalSize+=fklGetVMTypeSize(fklGetVMTypeUnion(memberTypes[i])),i++);
	VMTypeUnion* pst=&GlobTypeUnionList.ul[id-1];
	VMStructType* ost=(VMStructType*)GET_TYPES_PTR(pst->st);
	ost=(VMStructType*)realloc(ost,sizeof(VMStructType)+num*sizeof(VMStructMember));
	FAKE_ASSERT(ost,"initVMStructTypeId",__FILE__,__LINE__);
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

static void initVMUnionTypeId(TypeId_t id,const char* unionName,uint32_t num,Sid_t* symbols,TypeId_t* memberTypes)
{
	size_t maxSize=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklGetVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
			maxSize=curSize;
	}
	VMTypeUnion* put=&GlobTypeUnionList.ul[id-1];
	VMUnionType* out=(VMUnionType*)GET_TYPES_PTR(put->ut);
	out=(VMUnionType*)realloc(out,sizeof(VMUnionType)+num*sizeof(VMStructMember));
	FAKE_ASSERT(out,"initVMUnionTypeId",__FILE__,__LINE__);
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

static TypeId_t addToGlobTypeUnionList(VMTypeUnion type)
{
	GlobTypeUnionList.num+=1;
	size_t num=GlobTypeUnionList.num;
	GlobTypeUnionList.ul=(VMTypeUnion*)realloc(GlobTypeUnionList.ul,sizeof(VMTypeUnion)*num);
	FAKE_ASSERT(GlobTypeUnionList.ul,"addToGlobTypeUnionList",__FILE__,__LINE__);
	GlobTypeUnionList.ul[num-1]=type;
	return num;
}

/*genTypeId functions list*/
static TypeId_t genArrayTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	AST_cptr* numCptr=fklNextCptr(compositeDataHead);
	if(!numCptr||numCptr->type!=ATM||numCptr->u.atom->type!=IN32)
		return 0;
	AST_cptr* typeCptr=fklNextCptr(numCptr);
	if(!typeCptr)
		return 0;
	TypeId_t type=fklGenDefTypesUnion(typeCptr,otherTypes);
	if(!type)
		return type;
	if(fklGetCptrCdr(typeCptr)->type!=NIL)
		return 0;
	return fklNewVMArrayType(type,numCptr->u.atom->value.in32);
}

static TypeId_t genPtrTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	AST_cptr* ptrTypeCptr=fklNextCptr(compositeDataHead);
	if(ptrTypeCptr->type==ATM&&ptrTypeCptr->u.atom->type!=SYM)
		return 0;
	TypeId_t pType=fklGenDefTypesUnion(ptrTypeCptr,otherTypes);
	if(!pType)
		return pType;
	if(fklGetCptrCdr(ptrTypeCptr)->type!=NIL)
		return 0;
	return fklNewVMPtrType(pType);
}

static int isInPtrDeclare(AST_cptr* compositeDataHead)
{
	AST_pair* outerPair=compositeDataHead->outer;
	AST_pair* outerPrevPair=outerPair->prev;
	if(outerPrevPair)
	{
		AST_cptr* outerTypeHead=fklPrevCptr(&outerPrevPair->car);
		if(outerTypeHead)
			return outerTypeHead->type==ATM&&outerTypeHead->u.atom->type==SYM&&!strcmp(outerTypeHead->u.atom->value.str,"ptr");
	}
	return 0;
}

static TypeId_t genStructTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	char* structName=NULL;
	AST_cptr* structNameCptr=fklNextCptr(compositeDataHead);
	uint32_t num=0;
	AST_cptr* memberCptr=NULL;
	if(structNameCptr->type==ATM)
	{
		if(structNameCptr->u.atom->type==SYM)
			structName=structNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=fklNextCptr(structNameCptr);
		structNameCptr=memberCptr;
	}
	else
		memberCptr=structNameCptr;
	TypeId_t retval=0;
	if(memberCptr==NULL&&structName!=NULL)
	{
		TypeId_t i=0;
		uint32_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			VMTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(GET_TYPES_TAG(typeUnion.all)==STRUCT_TYPE_TAG)
			{
				VMStructType* st=(VMStructType*)GET_TYPES_PTR(typeUnion.st);
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
	Sid_t* memberSymbolList=NULL;
	TypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(Sid_t*)malloc(sizeof(Sid_t)*num);
		memberTypeList=(TypeId_t*)malloc(sizeof(TypeId_t)*num);
		FAKE_ASSERT(memberTypeList&&memberCptr,"fklGenDefTypesUnion",__FILE__,__LINE__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=fklNextCptr(memberCptr))
		{
			AST_cptr* cdr=fklGetCptrCdr(memberCptr);
			if(cdr->type!=PAIR&&cdr->type!=NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			AST_cptr* memberName=fklGetFirstCptr(memberCptr);
			if(memberName->type!=ATM||memberName->u.atom->type!=SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			Sid_t symbol=fklAddSymbolToGlob(memberName->u.atom->value.str)->id;
			if(fklNextCptr(fklNextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			TypeId_t type=fklGenDefTypesUnion(fklNextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(fklGetCptrCdr(fklNextCptr(memberName))->type!=NIL)
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

static TypeId_t genUnionTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	char* unionName=NULL;
	AST_cptr* unionNameCptr=fklNextCptr(compositeDataHead);
	uint32_t num=0;
	AST_cptr* memberCptr=NULL;
	if(unionNameCptr->type==ATM)
	{
		if(unionNameCptr->u.atom->type==SYM)
			unionName=unionNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=fklNextCptr(unionNameCptr);
		unionNameCptr=memberCptr;
	}
	else
		memberCptr=unionNameCptr;
	TypeId_t retval=0;
	if(memberCptr==NULL&&unionName!=NULL)
	{
		TypeId_t i=0;
		uint32_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			VMTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(GET_TYPES_TAG(typeUnion.all)==UNION_TYPE_TAG)
			{
				VMUnionType* ut=(VMUnionType*)GET_TYPES_PTR(typeUnion.st);
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
	Sid_t* memberSymbolList=NULL;
	TypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(Sid_t*)malloc(sizeof(Sid_t)*num);
		memberTypeList=(TypeId_t*)malloc(sizeof(TypeId_t)*num);
		FAKE_ASSERT(memberTypeList&&memberCptr,"fklGenDefTypesUnion",__FILE__,__LINE__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=fklNextCptr(memberCptr))
		{
			AST_cptr* cdr=fklGetCptrCdr(memberCptr);
			if(cdr->type!=PAIR&&cdr->type!=NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			AST_cptr* memberName=fklGetFirstCptr(memberCptr);
			if(memberName->type!=ATM||memberName->u.atom->type!=SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			Sid_t symbol=fklAddSymbolToGlob(memberName->u.atom->value.str)->id;
			if(fklNextCptr(fklNextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			TypeId_t type=fklGenDefTypesUnion(fklNextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(fklGetCptrCdr(fklNextCptr(memberName))->type!=NIL)
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

static TypeId_t genFuncTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	TypeId_t rtype=0;
	AST_cptr* argCptr=fklNextCptr(compositeDataHead);
	if(!argCptr||(argCptr->type!=PAIR&&argCptr->type!=NIL))
		return 0;
	AST_cptr* rtypeCptr=fklNextCptr(argCptr);
	if(rtypeCptr)
	{
		TypeId_t tmp=fklGenDefTypesUnion(rtypeCptr,otherTypes);
		if(!tmp)
			return 0;
		VMTypeUnion tu=fklGetVMTypeUnion(tmp);
		if(GET_TYPES_TAG(tu.all)!=NATIVE_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=PTR_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=ARRAY_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FUNC_TYPE_TAG)
			return 0;
		rtype=tmp;
	}
	uint32_t i=0;
	AST_cptr* firArgCptr=fklGetFirstCptr(argCptr);
	for(;firArgCptr;firArgCptr=fklNextCptr(firArgCptr),i++);
	TypeId_t* atypes=(TypeId_t*)malloc(sizeof(TypeId_t)*i);
	FAKE_ASSERT(atypes,"fklGenDefTypesUnion",__FILE__,__LINE__);
	for(i=0,firArgCptr=fklGetFirstCptr(argCptr);firArgCptr;firArgCptr=fklNextCptr(firArgCptr),i++)
	{
		TypeId_t tmp=fklGenDefTypesUnion(firArgCptr,otherTypes);
		if(!tmp)
		{
			free(atypes);
			return 0;
		}
		VMTypeUnion tu=fklGetVMTypeUnion(tmp);
		if(GET_TYPES_TAG(tu.all)!=NATIVE_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=PTR_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=ARRAY_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FUNC_TYPE_TAG)
		{
			free(atypes);
			return 0;
		}
		AST_cptr* cdr=fklGetCptrCdr(firArgCptr);
		if(!tmp||(cdr->type!=PAIR&&cdr->type!=NIL))
		{
			free(atypes);
			return 0;
		}
		atypes[i]=tmp;
	}
	TypeId_t retval=fklNewVMFuncType(rtype,i,atypes);
	free(atypes);
	return retval;
}

/*------------------------*/

static CRL* newCRL(VMpair* pair,int32_t count)
{
	CRL* tmp=(CRL*)malloc(sizeof(CRL));
	FAKE_ASSERT(tmp,"newCRL",__FILE__,__LINE__);
	tmp->pair=pair;
	tmp->count=count;
	tmp->next=NULL;
	return tmp;
}

static int32_t findCRLcount(VMpair* pair,CRL* h)
{
	for(;h;h=h->next)
	{
		if(h->pair==pair)
			return h->count;
	}
	return -1;
}

static VMpair* hasSameVMpair(VMpair* begin,VMpair* other,CRL* h)
{
	VMpair* tmpPair=NULL;
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

VMpair* isCircularReference(VMpair* begin,CRL* h)
{
	VMpair* tmpPair=NULL;
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

int8_t isInTheCircle(VMpair* obj,VMpair* begin,VMpair* curPair)
{
	if(obj==curPair)
		return 1;
	if((IS_PAIR(curPair->car)&&begin==curPair->car->u.pair)||(IS_PAIR(curPair->cdr)&&begin==curPair->cdr->u.pair))
		return 0;
	return ((IS_PAIR(curPair->car))&&isInTheCircle(obj,begin,curPair->car->u.pair))||((IS_PAIR(curPair->cdr))&&isInTheCircle(obj,begin,curPair->cdr->u.pair));
}



pthread_mutex_t VMenvGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;

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

VMproc* fklNewVMproc(uint32_t scp,uint32_t cpc)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	FAKE_ASSERT(tmp,"fklNewVMproc",__FILE__,__LINE__);
	tmp->prevEnv=NULL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	return tmp;
}

VMvalue* fklCopyVMvalue(VMvalue* obj,VMheap* heap)
{
	ComStack* s1=fklNewComStack(32);
	ComStack* s2=fklNewComStack(32);
	VMvalue* tmp=VM_NIL;
	fklPushComStack(obj,s1);
	fklPushComStack(&tmp,s2);
	while(!fklIsComStackEmpty(s1))
	{
		VMvalue* root=fklPopComStack(s1);
		VMptrTag tag=GET_TAG(root);
		VMvalue** root1=fklPopComStack(s2);
		switch(tag)
		{
			case NIL_TAG:
			case IN32_TAG:
			case SYM_TAG:
			case CHR_TAG:
				*root1=root;
				break;
			case PTR_TAG:
				{
					ValueType type=root->type;
					switch(type)
					{
						case DBL:
							*root1=fklNewVMvalue(DBL,root->u.dbl,heap);
							break;
						case BYTS:
							*root1=fklNewVMvalue(BYTS,fklNewVMByts(root->u.byts->size,root->u.byts->str),heap);
							break;
						case STR:
							*root1=fklNewVMvalue(STR,fklCopyStr(root->u.str),heap);
							break;
						case CONT:
						case PRC:
						case FP:
						case DLL:
						case DLPROC:
						case ERR:
							*root1=root;
							break;
						case CHAN:
							{
								VMChanl* objCh=root->u.chan;
								VMChanl* tmpCh=fklNewVMChanl(objCh->max);
								QueueNode* cur=objCh->messages->head;
								for(;cur;cur=cur->next)
								{
									void* tmp=VM_NIL;
									fklPushComQueue(tmp,tmpCh->messages);
									fklPushComStack(cur->data,s1);
									fklPushComStack(tmp,s2);
								}
								*root1=fklNewVMvalue(CHAN,tmpCh,heap);
							}
							break;
						case PAIR:
							*root1=fklNewVMvalue(PAIR,fklNewVMpair(),heap);
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

VMvalue* fklNewVMvalue(ValueType type,void* pValue,VMheap* heap)
{
	switch((int)type)
	{
		case NIL:
			return VM_NIL;
			break;
		case CHR:
			return MAKE_VM_CHR(pValue);
			break;
		case IN32:
			return MAKE_VM_IN32(pValue);
			break;
		case SYM:
			return MAKE_VM_SYM(pValue);
			break;
		default:
			{
				VMvalue* tmp=(VMvalue*)malloc(sizeof(VMvalue));
				FAKE_ASSERT(tmp,"fklNewVMvalue",__FILE__,__LINE__);
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
					case DBL:
						tmp->u.dbl=fklCopyMemory(pValue,sizeof(double));
						break;
					case IN64:
						tmp->u.in64=fklCopyMemory(pValue,sizeof(int64_t));
						break;
					case STR:
						tmp->u.str=pValue;break;
					case PAIR:
						tmp->u.pair=pValue;break;
					case PRC:
						tmp->u.prc=pValue;break;
					case BYTS:
						tmp->u.byts=pValue;break;
					case CONT:
						tmp->u.cont=pValue;break;
					case CHAN:
						tmp->u.chan=pValue;break;
					case FP:
						tmp->u.fp=pValue;break;
					case DLL:
						tmp->u.dll=pValue;break;
					case DLPROC:
						tmp->u.dlproc=pValue;break;
					case FLPROC:
						tmp->u.flproc=pValue;break;
					case ERR:
						tmp->u.err=pValue;break;
					case CHF:
						tmp->u.chf=pValue;break;
					case MEM:
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

VMvalue* fklNewTrueValue(VMheap* heap)
{
	VMvalue* tmp=fklNewVMvalue(IN32,(VMptr)1,heap);
	return tmp;
}

VMvalue* fklNewNilValue(VMheap* heap)
{
	VMvalue* tmp=fklNewVMvalue(NIL,NULL,heap);
	return tmp;
}

VMvalue* fklGetTopValue(VMstack* stack)
{
	return stack->values[stack->tp-1];
}

VMvalue* fklGetValue(VMstack* stack,int32_t place)
{
	return stack->values[place];
}

VMvalue* fklGetVMpairCar(VMvalue* obj)
{
	return obj->u.pair->car;
}

VMvalue* fklGetVMpairCdr(VMvalue* obj)
{
	return obj->u.pair->cdr;
}

int fklVMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	ComStack* s1=fklNewComStack(32);
	ComStack* s2=fklNewComStack(32);
	fklPushComStack(fir,s1);
	fklPushComStack(sec,s2);
	int r=1;
	while(!fklIsComStackEmpty(s1))
	{
		VMvalue* root1=fklPopComStack(s1);
		VMvalue* root2=fklPopComStack(s2);
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
				case DBL:
					r=(fabs(*root1->u.dbl-*root2->u.dbl)==0);
					break;
				case IN64:
					r=(*root1->u.in64-*root2->u.in64)==0;
				case STR:
					r=!strcmp(root1->u.str,root2->u.str);
					break;
				case BYTS:
					r=fklEqVMByts(root1->u.byts,root2->u.byts);
					break;
				case PAIR:
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

int subfklVMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	return fir==sec;
}

int fklNumcmp(VMvalue* fir,VMvalue* sec)
{
	if(GET_TAG(fir)==GET_TAG(sec)&&GET_TAG(fir)==IN32_TAG)
		return fir==sec;
	else
	{
		double first=(GET_TAG(fir)==IN32_TAG)?GET_IN32(fir):*fir->u.dbl;
		double second=(GET_TAG(sec)==IN32_TAG)?GET_IN32(sec):*sec->u.dbl;
		return fabs(first-second)==0;
	}
}

VMenv* fklNewVMenv(VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	FAKE_ASSERT(tmp,"fklNewVMenv",__FILE__,__LINE__);
	tmp->num=0;
	tmp->list=NULL;
	tmp->prev=prev;
	fklIncreaseVMenvRefcount(prev);
	tmp->refcount=0;
	return tmp;
}

void fklIncreaseVMenvRefcount(VMenv* env)
{
	INCREASE_REFCOUNT(VMenv,env);
}

void fklDecreaseVMenvRefcount(VMenv* env)
{
	DECREASE_REFCOUNT(VMenv,env);
}

VMpair* fklNewVMpair(void)
{
	VMpair* tmp=(VMpair*)malloc(sizeof(VMpair));
	FAKE_ASSERT(tmp,"fklNewVMpair",__FILE__,__LINE__);
	tmp->car=VM_NIL;
	tmp->cdr=VM_NIL;
	return tmp;
}

VMvalue* fklCastCptrVMvalue(AST_cptr* objCptr,VMheap* heap)
{
	ComStack* s1=fklNewComStack(32);
	ComStack* s2=fklNewComStack(32);
	VMvalue* tmp=VM_NIL;
	fklPushComStack(objCptr,s1);
	fklPushComStack(&tmp,s2);
	while(!fklIsComStackEmpty(s1))
	{
		AST_cptr* root=fklPopComStack(s1);
		VMvalue** root1=fklPopComStack(s2);
		if(root->type==ATM)
		{
			AST_atom* tmpAtm=root->u.atom;
			switch(tmpAtm->type)
			{
				case IN32:
					*root1=MAKE_VM_IN32(tmpAtm->value.in32);
					break;
				case CHR:
					*root1=MAKE_VM_CHR(tmpAtm->value.chr);
					break;
				case SYM:
					*root1=MAKE_VM_SYM(fklAddSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case DBL:
					*root1=fklNewVMvalue(DBL,&tmpAtm->value.dbl,heap);
					break;
				case BYTS:
					*root1=fklNewVMvalue(BYTS,fklNewVMByts(tmpAtm->value.byts.size,tmpAtm->value.byts.str),heap);
					break;
				case STR:
					*root1=fklNewVMvalue(STR,fklCopyStr(tmpAtm->value.str),heap);
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==PAIR)
		{
			AST_pair* objPair=root->u.pair;
			VMpair* tmpPair=fklNewVMpair();
			*root1=fklNewVMvalue(PAIR,tmpPair,heap);
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

VMByts* fklNewVMByts(size_t size,uint8_t* str)
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts)+size);
	FAKE_ASSERT(tmp,"fklNewVMByts",__FILE__,__LINE__);
	tmp->size=size;
	memcpy(tmp->str,str,size);
	return tmp;
}

VMByts* fklCopyVMByts(const VMByts* obj)
{
	if(obj==NULL)return NULL;
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts)+obj->size);
	FAKE_ASSERT(tmp,"fklCopyVMByts",__FILE__,__LINE__);
	memcpy(tmp->str,obj->str,obj->size);
	tmp->size=obj->size;
	return tmp;
}

void fklVMBytsCat(VMByts** fir,const VMByts* sec)
{
	size_t firSize=(*fir)->size;
	size_t secSize=sec->size;
	*fir=(VMByts*)realloc(*fir,sizeof(VMByts)+(firSize+secSize)*sizeof(uint8_t));
	FAKE_ASSERT(*fir,"fklVMBytsCat",__FILE__,__LINE__);
	(*fir)->size=firSize+secSize;
	memcpy((*fir)->str+firSize,sec->str,secSize);
}
VMByts* fklNewEmptyVMByts()
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts));
	FAKE_ASSERT(tmp,"fklNewEmptyVMByts",__FILE__,__LINE__);
	tmp->size=0;
	return tmp;
}

uint8_t* fklCopyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"fklCopyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

VMproc* fklCopyVMproc(VMproc* obj,VMheap* heap)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	FAKE_ASSERT(tmp,"fklCopyVMproc",__FILE__,__LINE__);
	tmp->scp=obj->scp;
	tmp->cpc=obj->cpc;
	tmp->prevEnv=fklCopyVMenv(obj->prevEnv,heap);
	return tmp;
}

VMenv* fklCopyVMenv(VMenv* objEnv,VMheap* heap)
{
	VMenv* tmp=fklNewVMenv(NULL);
	tmp->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*objEnv->num);
	FAKE_ASSERT(tmp->list,"fklCopyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->num;i++)
	{
		VMenvNode* node=objEnv->list[i];
		VMvalue* v=fklCopyVMvalue(node->value,heap);
		VMenvNode* tmp=fklNewVMenvNode(v,node->id);
		objEnv->list[i]=tmp;
	}
	tmp->prev=(objEnv->prev->refcount==0)?fklCopyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void fklFreeVMproc(VMproc* proc)
{
	if(proc->prevEnv)
		fklFreeVMenv(proc->prevEnv);
	free(proc);
}

void fklFreeVMenv(VMenv* obj)
{
	while(obj!=NULL)
	{
		if(!obj->refcount)
		{
			VMenv* prev=obj;
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

VMvalue* fklPopVMstack(VMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	VMvalue* tmp=fklGetTopValue(stack);
	stack->tp-=1;
	return tmp;
}

VMenv* fklCastPreEnvToVMenv(PreEnv* pe,VMenv* prev,VMheap* heap)
{
	int32_t size=0;
	PreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	VMenv* tmp=fklNewVMenv(prev);
	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
	{
		VMvalue* v=fklCastCptrVMvalue(&tmpDef->obj,heap);
		VMenvNode* node=fklNewVMenvNode(v,fklAddSymbolToGlob(tmpDef->symbol)->id);
		fklAddVMenvNode(node,tmp);
	}
	return tmp;
}

VMcontinuation* fklNewVMcontinuation(VMstack* stack,ComStack* rstack,ComStack* tstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	FAKE_ASSERT(tmp,"fklNewVMcontinuation",__FILE__,__LINE__);
	VMrunnable* state=(VMrunnable*)malloc(sizeof(VMrunnable)*size);
	FAKE_ASSERT(state,"fklNewVMcontinuation",__FILE__,__LINE__);
	int32_t tbnum=tstack->top;
	VMTryBlock* tb=(VMTryBlock*)malloc(sizeof(VMTryBlock)*tbnum);
	FAKE_ASSERT(tb,"fklNewVMcontinuation",__FILE__,__LINE__);
	tmp->stack=fklCopyStack(stack);
	tmp->num=size;
	for(;i<size;i++)
	{
		VMrunnable* cur=rstack->data[i];
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
		VMTryBlock* cur=tstack->data[i];
		tb[i].sid=cur->sid;
		ComStack* hstack=cur->hstack;
		int32_t handlerNum=hstack->top;
		ComStack* curHstack=fklNewComStack(handlerNum);
		int32_t j=0;
		for(;j<handlerNum;j++)
		{
			VMerrorHandler* curH=hstack->data[i];
			VMerrorHandler* h=fklNewVMerrorHandler(curH->type,curH->proc.scp,curH->proc.cpc);
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
	VMstack* stack=cont->stack;
	VMrunnable* state=cont->state;
	VMTryBlock* tb=cont->tb;
	free(stack->tpst);
	free(stack->values);
	free(stack);
	for(;i<size;i++)
		fklFreeVMenv(state[i].localenv);
	for(i=0;i<tbsize;i++)
	{
		ComStack* hstack=tb[i].hstack;
		while(!fklIsComStackEmpty(hstack))
		{
			VMerrorHandler* h=fklPopComStack(hstack);
			fklFreeVMerrorHandler(h);
		}
		fklFreeComStack(hstack);
	}
	free(tb);
	free(state);
	free(cont);
}

VMstack* fklCopyStack(VMstack* stack)
{
	int32_t i=0;
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	FAKE_ASSERT(tmp,"fklCopyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(VMvalue**)malloc(sizeof(VMvalue*)*(tmp->size));
	FAKE_ASSERT(tmp->values,"fklCopyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FAKE_ASSERT(tmp->tpst,"fklCopyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

VMenvNode* fklNewVMenvNode(VMvalue* value,int32_t id)
{
	VMenvNode* tmp=(VMenvNode*)malloc(sizeof(VMenvNode));
	FAKE_ASSERT(tmp,"fklNewVMenvNode",__FILE__,__LINE__);
	tmp->id=id;
	tmp->value=value;
	return tmp;
}

VMenvNode* fklAddVMenvNode(VMenvNode* node,VMenv* env)
{
	if(!env->list)
	{
		env->num=1;
		env->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*1);
		FAKE_ASSERT(env->list,"fklAddVMenvNode",__FILE__,__LINE__);
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
		env->list=(VMenvNode**)realloc(env->list,sizeof(VMenvNode*)*env->num);
		FAKE_ASSERT(env->list,"fklAddVMenvNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		env->list[mid]=node;
	}
	return node;
}

VMenvNode* fklFindVMenvNode(Sid_t id,VMenv* env)
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

void fklFreeVMenvNode(VMenvNode* node)
{
	free(node);
}


VMChanl* fklNewVMChanl(int32_t maxSize)
{
	VMChanl* tmp=(VMChanl*)malloc(sizeof(VMChanl));
	FAKE_ASSERT(tmp,"fklNewVMChanl",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->messages=fklNewComQueue();
	tmp->sendq=fklNewComQueue();
	tmp->recvq=fklNewComQueue();
	return tmp;
}

int32_t fklGetNumVMChanl(VMChanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=fklLengthComQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

void fklFreeVMChanl(VMChanl* ch)
{
	pthread_mutex_destroy(&ch->lock);
	fklFreeComQueue(ch->messages);
	QueueNode* head=ch->sendq->head;
	for(;head;head=head->next)
		fklFreeSendT(head->data);
	for(head=ch->recvq->head;head;head=head->next)
		fklFreeRecvT(head->data);
	fklFreeComQueue(ch->sendq);
	fklFreeComQueue(ch->recvq);
	free(ch);
}

VMChanl* fklCopyVMChanl(VMChanl* ch,VMheap* heap)
{
	VMChanl* tmpCh=fklNewVMChanl(ch->max);
	QueueNode* cur=ch->messages->head;
	ComQueue* tmp=fklNewComQueue();
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

DllHandle* fklNewVMDll(const char* dllName)
{
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	size_t len=strlen(dllName)+strlen(filetype)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(realDllName,"fklNewVMDll",__FILE__,__LINE__);
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
	DllHandle handle=LoadLibrary(rpath);
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
	DllHandle handle=dlopen(rpath,RTLD_LAZY);
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

void fklFreeVMDll(DllHandle* dll)
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

void* fklGetAddress(const char* funcname,DllHandle dlhandle)
{
	void* pfunc=NULL;
#ifdef _WIN32
		pfunc=GetProcAddress(dlhandle,funcname);
#else
		pfunc=dlsym(dlhandle,funcname);
#endif
	return pfunc;
}

VMDlproc* fklNewVMDlproc(DllFunc address,VMvalue* dll)
{
	VMDlproc* tmp=(VMDlproc*)malloc(sizeof(VMDlproc));
	FAKE_ASSERT(tmp,"fklNewVMDlproc",__FILE__,__LINE__);
	tmp->func=address;
	tmp->dll=dll;
	return tmp;
}

void fklFreeVMDlproc(VMDlproc* dlproc)
{
	free(dlproc);
}

VMFlproc* fklNewVMFlproc(TypeId_t type,void* func)
{
	VMFlproc* tmp=(VMFlproc*)malloc(sizeof(VMFlproc));
	FAKE_ASSERT(tmp,"fklNewVMDlproc",__FILE__,__LINE__);
	tmp->type=type;
	tmp->func=func;
	return tmp;
}

void fklFreeVMFlproc(VMFlproc* t)
{
	free(t);
}

VMerror* fklNewVMerror(const char* who,const char* type,const char* message)
{
	VMerror* t=(VMerror*)malloc(sizeof(VMerror));
	FAKE_ASSERT(t,"fklNewVMerror",__FILE__,__LINE__);
	t->who=fklCopyStr(who);
	t->type=fklAddSymbolToGlob(type)->id;
	t->message=fklCopyStr(message);
	return t;
}

VMerror* fklNewVMerrorWithSid(const char* who,Sid_t type,const char* message)
{
	VMerror* t=(VMerror*)malloc(sizeof(VMerror));
	FAKE_ASSERT(t,"fklNewVMerror",__FILE__,__LINE__);
	t->who=fklCopyStr(who);
	t->type=type;
	t->message=fklCopyStr(message);
	return t;
}

void fklFreeVMerror(VMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

RecvT* fklNewRecvT(FakeVM* v)
{
	RecvT* tmp=(RecvT*)malloc(sizeof(RecvT));
	FAKE_ASSERT(tmp,"fklNewRecvT",__FILE__,__LINE__);
	tmp->v=v;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeRecvT(RecvT* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

SendT* fklNewSendT(VMvalue* m)
{
	SendT* tmp=(SendT*)malloc(sizeof(SendT));
	FAKE_ASSERT(tmp,"fklNewSendT",__FILE__,__LINE__);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeSendT(SendT* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

void fklChanlRecv(RecvT* r,VMChanl* ch)
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
		SendT* s=fklPopComQueue(ch->sendq);
		if(s)
			pthread_cond_signal(&s->cond);
	}
	fklFreeRecvT(r);
	pthread_mutex_unlock(&ch->lock);
}

void fklChanlSend(SendT*s,VMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(fklLengthComQueue(ch->recvq))
	{
		RecvT* r=fklPopComQueue(ch->recvq);
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

VMTryBlock* fklNewVMTryBlock(Sid_t sid,uint32_t tp,long int rtp)
{
	VMTryBlock* t=(VMTryBlock*)malloc(sizeof(VMTryBlock));
	FAKE_ASSERT(t,"fklNewVMTryBlock",__FILE__,__LINE__);
	t->sid=sid;
	t->hstack=fklNewComStack(32);
	t->tp=tp;
	t->rtp=rtp;
	return t;
}

void fklFreeVMTryBlock(VMTryBlock* b)
{
	ComStack* hstack=b->hstack;
	while(!fklIsComStackEmpty(hstack))
	{
		VMerrorHandler* h=fklPopComStack(hstack);
		fklFreeVMerrorHandler(h);
	}
	fklFreeComStack(b->hstack);
	free(b);
}

VMerrorHandler* fklNewVMerrorHandler(Sid_t type,uint32_t scp,uint32_t cpc)
{
	VMerrorHandler* t=(VMerrorHandler*)malloc(sizeof(VMerrorHandler));
	FAKE_ASSERT(t,"fklNewVMerrorHandler",__FILE__,__LINE__);
	t->type=type;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	return t;
}

void fklFreeVMerrorHandler(VMerrorHandler* h)
{
	fklFreeVMenv(h->proc.prevEnv);
	free(h);
}

int fklRaiseVMerror(VMvalue* ev,FakeVM* exe)
{
	VMerror* err=ev->u.err;
	while(!fklIsComStackEmpty(exe->tstack))
	{
		VMTryBlock* tb=fklTopComStack(exe->tstack);
		VMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!fklIsComStackEmpty(tb->hstack))
		{
			VMerrorHandler* h=fklPopComStack(tb->hstack);
			if(h->type==err->type)
			{
				long int increment=exe->rstack->top-tb->rtp;
				unsigned int i=0;
				for(;i<increment;i++)
				{
					VMrunnable* runnable=fklPopComStack(exe->rstack);
					fklFreeVMenv(runnable->localenv);
					free(runnable);
				}
				VMrunnable* prevRunnable=fklTopComStack(exe->rstack);
				VMrunnable* r=fklNewVMrunnable(&h->proc);
				r->localenv=fklNewVMenv(prevRunnable->localenv);
				VMenv* curEnv=r->localenv;
				Sid_t idOfError=tb->sid;
				VMenvNode* errorNode=fklFindVMenvNode(idOfError,curEnv);
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

VMrunnable* fklNewVMrunnable(VMproc* code)
{
	VMrunnable* tmp=(VMrunnable*)malloc(sizeof(VMrunnable));
	FAKE_ASSERT(tmp,"fklNewVMrunnable",__FILE__,__LINE__);
	if(code)
	{
		tmp->cp=code->scp;
		tmp->scp=code->scp;
		tmp->cpc=code->cpc;
	}
	tmp->mark=0;
	return tmp;
}

char* fklGenInvalidSymbolErrorMessage(const char* str,VMrunnable* r,FakeVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=GlobSymbolTable.idl[node->fid]->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
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

char* fklGenErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=GlobSymbolTable.idl[node->fid]->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
	switch(type)
	{
		case WRONGARG:
			t=fklStrCat(t,"Wrong arguement ");
			break;
		case STACKERROR:
			t=fklStrCat(t,"Stack error ");
			break;
		case TOOMANYARG:
			t=fklStrCat(t,"Too many arguements ");
			break;
		case TOOFEWARG:
			t=fklStrCat(t,"Too few arguements ");
			break;
		case CANTCREATETHREAD:
			t=fklStrCat(t,"Can't create thread ");
			break;
		case SYMUNDEFINE:
			t=fklStrCat(t,"Symbol ");
			t=fklStrCat(t,GlobSymbolTable.idl[fklGetSymbolIdInByteCode(exe->code+r->cp)]->symbol);
			t=fklStrCat(t," is undefined ");
			break;
		case INVOKEERROR:
			t=fklStrCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure ");
			break;
		case LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t," ");
			break;
		case DIVZERROERROR:
			t=fklStrCat(t,"Divided by zero ");
			break;
		case FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case INVALIDASSIGN:
			t=fklStrCat(t,"Invalid assign ");
			break;
		case INVALIDACCESS:
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
		case FAKE_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FAKE_POP_VAR:
		case FAKE_POP_ARG:
		case FAKE_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}

int fklResBp(VMstack* stack)
{
	if(stack->tp>stack->bp)
		return TOOMANYARG;
	VMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=GET_IN32(prevBp);
	stack->tp-=1;
	return 0;
}

void fklPrincVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	VMpair* cirPair=NULL;
	int32_t CRLcount=-1;
	int8_t isInCir=0;
	VMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case NIL_TAG:
			fprintf(fp,"()");
			break;
		case IN32_TAG:
			fprintf(fp,"%d",GET_IN32(objValue));
			break;
		case CHR_TAG:
			putc(GET_CHR(objValue),fp);
			break;
		case SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case PTR_TAG:
			{
				switch(objValue->type)
				{
					case DBL:
						fprintf(fp,"%lf",*objValue->u.dbl);
						break;
					case IN64:
						fprintf(fp,"%ld",*objValue->u.in64);
						break;
					case STR:
						fprintf(fp,"%s",objValue->u.str);
						break;
					case MEM:
					case CHF:
						{
							VMMem* mem=objValue->u.chf;
							TypeId_t type=mem->type;
							if(type>0&&type<=LastNativeTypeId)
								PrintMemoryRefFuncList[type-1](mem->mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case PRC:
						fprintf(fp,"#<proc>");
						break;
					case PAIR:
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
							VMvalue* tmpValue=fklGetVMpairCar(objValue);
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
					case BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,0);
						break;
					case CONT:
						fprintf(fp,"#<cont>");
						break;
					case CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FP:
						fprintf(fp,"#<fp>");
						break;
					case DLL:
						fprintf(fp,"<#dll>");
						break;
					case DLPROC:
						fprintf(fp,"<#dlproc>");
						break;
					case FLPROC:
						fprintf(fp,"<#flproc>");
						break;
					case ERR:
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

void fklPrin1VMvalue(VMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	VMpair* cirPair=NULL;
	int8_t isInCir=0;
	int32_t CRLcount=-1;
	VMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case NIL_TAG:
			fprintf(fp,"()");
			break;
		case IN32_TAG:
			fprintf(fp,"%d",GET_IN32(objValue));
			break;
		case CHR_TAG:
			fklPrintRawChar(GET_CHR(objValue),fp);
			break;
		case SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case PTR_TAG:
			{
				switch(objValue->type)
				{
					case DBL:
						fprintf(fp,"%lf",*objValue->u.dbl);
						break;
					case IN64:
						fprintf(fp,"%ld",*objValue->u.in64);
						break;
					case STR:
						fklPrintRawString(objValue->u.str,fp);
						break;
					case MEM:
					case CHF:
						{
							VMMem* mem=objValue->u.chf;
							TypeId_t type=mem->type;
							if(type>0&&type<=LastNativeTypeId)
								PrintMemoryRefFuncList[type-1](mem->mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case PRC:
						fprintf(fp,"#<proc>");
						break;
					case PAIR:
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
							VMvalue* tmpValue=fklGetVMpairCar(objValue);
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
					case BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,1);
						break;
					case CONT:
						fprintf(fp,"#<cont>");
						break;
					case CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FP:
						fprintf(fp,"#<fp>");
						break;
					case DLL:
						fprintf(fp,"<#dll>");
						break;
					case DLPROC:
						fprintf(fp,"<#dlproc>");
						break;
					case FLPROC:
						fprintf(fp,"<#flproc>");
						break;
					case ERR:
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

int fklEqVMByts(const VMByts* fir,const VMByts* sec)
{
	if(fir->size!=sec->size)
		return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

VMvalue* fklGET_VAL(VMvalue* P,VMheap* heap)
{
	if(P)
	{
		if(IS_REF(P))
			return *(VMvalue**)(GET_PTR(P));
		else if(IS_CHF(P))
		{
			VMvalue* t=NULL;
			VMMem* mem=P->u.chf;
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

int fklSET_REF(VMvalue* P,VMvalue* V)
{
	if(IS_MEM(P)||IS_CHF(P))
	{
		VMMem* mem=P->u.chf;
		if(mem->type<=0)
			return 1;
		else if(mem->type>LastNativeTypeId)
		{
			if(!IS_IN32(V)&&!IS_IN64(V))
				return 1;
			mem->mem=(uint8_t*)(IS_IN32(V)?GET_IN32(V):*V->u.in64);
		}
		else if(MemorySeterList[mem->type-1](mem->mem,V))
			return 1;
		return 0;
	}
	else if(IS_REF(P))
	{
		*(VMvalue**)GET_PTR(P)=V;
		return 0;
	}
	else
		return 1;
}

TypeId_t fklNewVMNativeType(Sid_t type,size_t size,size_t align)
{
	VMNativeType* tmp=(VMNativeType*)malloc(sizeof(VMNativeType));
	FAKE_ASSERT(tmp,"fklNewVMNativeType",__FILE__,__LINE__);
	tmp->type=type;
	tmp->align=align;
	tmp->size=size;
	return addToGlobTypeUnionList((VMTypeUnion)MAKE_NATIVE_TYPE(tmp));
}

void fklFreeVMNativeType(VMNativeType* obj)
{
	free(obj);
}

VMTypeUnion fklGetVMTypeUnion(TypeId_t t)
{
	return GlobTypeUnionList.ul[t-1];
}

size_t fklGetVMTypeSizeWithTypeId(TypeId_t t)
{
	return fklGetVMTypeSize(fklGetVMTypeUnion(t));
}

size_t fklGetVMTypeSize(VMTypeUnion t)
{
	DefTypeTag tag=GET_TYPES_TAG(t.all);
	t.all=GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case NATIVE_TYPE_TAG:
			return t.nt->size;
			break;
		case ARRAY_TYPE_TAG:
			return t.at->totalSize;
			break;
		case PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case STRUCT_TYPE_TAG:
			return t.st->totalSize;
			break;
		case UNION_TYPE_TAG:
			return t.ut->maxSize;
			break;
		case FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

size_t fklGetVMTypeAlign(VMTypeUnion t)
{
	DefTypeTag tag=GET_TYPES_TAG(t.all);
	t.all=GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case NATIVE_TYPE_TAG:
			return t.nt->align;
			break;
		case ARRAY_TYPE_TAG:
			return t.at->align;
			break;
		case PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case STRUCT_TYPE_TAG:
			return t.st->align;
			break;
		case UNION_TYPE_TAG:
			return t.ut->align;
			break;
		case FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

TypeId_t fklNewVMArrayType(TypeId_t type,size_t num)
{
	TypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		VMTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(GET_TYPES_TAG(tmpType.all)==ARRAY_TYPE_TAG)
		{
			VMArrayType* arrayType=(VMTypeUnion){.at=GET_TYPES_PTR(tmpType.all)}.at;
			if(arrayType->etype==type&&arrayType->num==num)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		VMArrayType* tmp=(VMArrayType*)malloc(sizeof(VMArrayType));
		FAKE_ASSERT(tmp,"fklNewVMArrayType",__FILE__,__LINE__);
		tmp->etype=type;
		tmp->num=num;
		tmp->totalSize=num*fklGetVMTypeSize(fklGetVMTypeUnion(type));
		tmp->align=fklGetVMTypeAlign(fklGetVMTypeUnion(type));
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_ARRAY_TYPE(tmp));
	}
	else
		return id;
}

void fklFreeVMArrayType(VMArrayType* obj)
{
	free(GET_TYPES_PTR(obj));
}

TypeId_t fklNewVMPtrType(TypeId_t type)
{
	TypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		VMTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(GET_TYPES_TAG(tmpType.all)==PTR_TYPE_TAG)
		{
			VMPtrType* ptrType=(VMTypeUnion){.pt=GET_TYPES_PTR(tmpType.all)}.pt;
			if(ptrType->ptype==type)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		VMPtrType* tmp=(VMPtrType*)malloc(sizeof(VMPtrType));
		FAKE_ASSERT(tmp,"fklNewVMPtrType",__FILE__,__LINE__);
		tmp->ptype=type;
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_PTR_TYPE(tmp));
	}
	return id;
}

void fklFreeVMPtrType(VMPtrType* obj)
{
	free(GET_TYPES_PTR(obj));
}

TypeId_t fklNewVMStructType(const char* structName,uint32_t num,Sid_t symbols[],TypeId_t memberTypes[])
{
	TypeId_t id=0;
	if(structName)
	{
		size_t i=0;
		size_t num=GlobTypeUnionList.num;
		Sid_t stype=fklAddSymbolToGlob(structName)->id;
		for(;i<num;i++)
		{
			VMTypeUnion tmpType=GlobTypeUnionList.ul[i];
			if(GET_TYPES_TAG(tmpType.all)==STRUCT_TYPE_TAG)
			{
				VMStructType* structType=(VMTypeUnion){.st=GET_TYPES_PTR(tmpType.all)}.st;
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
			VMTypeUnion tu=fklGetVMTypeUnion(memberTypes[i]);
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
		VMStructType* tmp=(VMStructType*)malloc(sizeof(VMStructType)+sizeof(VMStructMember)*num);
		FAKE_ASSERT(tmp,"fklNewVMStructType",__FILE__,__LINE__);
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
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_STRUCT_TYPE(tmp));
	}
	return id;
}

TypeId_t fklNewVMUnionType(const char* unionName,uint32_t num,Sid_t symbols[],TypeId_t memberTypes[])
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
	VMUnionType* tmp=(VMUnionType*)malloc(sizeof(VMUnionType)+sizeof(VMStructMember)*num);
	FAKE_ASSERT(tmp,"fklNewVMStructType",__FILE__,__LINE__);
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
	return addToGlobTypeUnionList((VMTypeUnion)MAKE_UNION_TYPE(tmp));
}

void fklFreeVMUnionType(VMUnionType* obj)
{
	free(GET_TYPES_PTR(obj));
}

void fklFreeVMStructType(VMStructType* obj)
{
	free(GET_TYPES_PTR(obj));
}

TypeId_t fklNewVMFuncType(TypeId_t rtype,uint32_t anum,TypeId_t atypes[])
{
	TypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		VMTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(GET_TYPES_TAG(tmpType.all)==FUNC_TYPE_TAG)
		{
			VMFuncType* funcType=(VMTypeUnion){.ft=GET_TYPES_PTR(tmpType.all)}.ft;
			if(funcType->rtype==rtype&&funcType->anum==anum&&!memcmp(funcType->atypes,atypes,sizeof(TypeId_t)*anum))
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		VMFuncType* tmp=(VMFuncType*)malloc(sizeof(VMFuncType)+sizeof(TypeId_t)*anum);
		FAKE_ASSERT(tmp,"fklNewVMFuncType",__FILE__,__LINE__);
		tmp->rtype=rtype;
		tmp->anum=anum;
		uint32_t i=0;
		for(;i<anum;i++)
			tmp->atypes[i]=atypes[i];
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_FUNC_TYPE(tmp));
	}
	return id;
}

void fklFreeVMFuncType(VMFuncType* obj)
{
	free(GET_TYPES_PTR(obj));
}

int fklAddDefTypes(VMDefTypes* otherTypes,Sid_t typeName,TypeId_t type)
{
	if(otherTypes->num==0)
	{
		otherTypes->num+=1;
		VMDefTypesNode* node=(VMDefTypesNode*)malloc(sizeof(VMDefTypesNode));
		otherTypes->u=(VMDefTypesNode**)malloc(sizeof(VMDefTypesNode*)*1);
		FAKE_ASSERT(otherTypes->u&&node,"fklAddDefTypes",__FILE__,__LINE__);
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
		otherTypes->u=(VMDefTypesNode**)realloc(otherTypes->u,sizeof(VMDefTypesNode*)*otherTypes->num);
		FAKE_ASSERT(otherTypes->u,"fklAddDefTypes",__FILE__,__LINE__);
		VMDefTypesNode* node=(VMDefTypesNode*)malloc(sizeof(VMDefTypesNode));
		FAKE_ASSERT(otherTypes->u&&node,"fklAddDefTypes",__FILE__,__LINE__);
		node->name=typeName;
		node->type=type;
		for(;i>mid;i--)
			otherTypes->u[i]=otherTypes->u[i-1];
		otherTypes->u[mid]=node;
	}
	return 0;
}

VMDefTypesNode* fklFindVMDefTypesNode(Sid_t typeName,VMDefTypes* otherTypes)
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
	//return (VMTypeUnion){.all=NULL};
}

TypeId_t fklGenDefTypes(AST_cptr* objCptr,VMDefTypes* otherTypes,Sid_t* typeName)
{
	AST_cptr* fir=fklNextCptr(fklGetFirstCptr(objCptr));
	if(fir->type!=ATM||fir->u.atom->type!=SYM)
		return 0;
	Sid_t typeId=fklAddSymbolToGlob(fir->u.atom->value.str)->id;
	if(isNativeType(typeId,otherTypes))
		return 0;
	*typeName=typeId;
	fir=fklNextCptr(fir);
	if(fir->type!=ATM&&fir->type!=PAIR)
		return 0;
	return fklGenDefTypesUnion(fir,otherTypes);
}

TypeId_t fklGenDefTypesUnion(AST_cptr* objCptr,VMDefTypes* otherTypes)
{
	if(!otherTypes)
		return 0;
	if(objCptr->type==ATM&&objCptr->u.atom->type==SYM)
	{
		VMDefTypesNode* n=fklFindVMDefTypesNode(fklAddSymbolToGlob(objCptr->u.atom->value.str)->id,otherTypes);
		if(!n)
			return 0;
		else
			return n->type;
	}
	else if(objCptr->type==PAIR)
	{
		AST_cptr* compositeDataHead=fklGetFirstCptr(objCptr);
		if(compositeDataHead->type!=ATM||compositeDataHead->u.atom->type!=SYM)
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

int isNativeType(Sid_t typeName,VMDefTypes* otherTypes)
{
	VMDefTypesNode* typeNode=fklFindVMDefTypesNode(typeName,otherTypes);
	return typeNode!=NULL&&GET_TYPES_TAG(fklGetVMTypeUnion(typeNode->type).all)==NATIVE_TYPE_TAG;
}

VMMem* fklNewVMMem(TypeId_t type,uint8_t* mem)
{
	VMMem* tmp=(VMMem*)malloc(sizeof(VMMem));
	FAKE_ASSERT(tmp,"fklNewVMMem",__FILE__,__LINE__);
	tmp->type=type;
	tmp->mem=mem;
	return tmp;
}

void fklInitNativeDefTypes(VMDefTypes* otherTypes)
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
		Sid_t typeName=fklAddSymbolToGlob(nativeTypeList[i].typeName)->id;
		size_t size=nativeTypeList[i].size;
		TypeId_t t=fklNewVMNativeType(typeName,size,size);
		if(!CharTypeId&&!strcmp("char",nativeTypeList[i].typeName))
			CharTypeId=t;
		//VMTypeUnion t={.nt=fklNewVMNativeType(typeName,size)};
		fklAddDefTypes(otherTypes,typeName,t);
	}
	Sid_t otherTypeName=fklAddSymbolToGlob("string")->id;
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
	TypeId_t i=0;
	TypeId_t num=GlobTypeUnionList.num;
	VMTypeUnion* ul=GlobTypeUnionList.ul;
	fwrite(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fwrite(&CharTypeId,sizeof(CharTypeId),1,fp);
	fwrite(&num,sizeof(num),1,fp);
	for(;i<num;i++)
	{
		VMTypeUnion tu=ul[i];
		DefTypeTag tag=GET_TYPES_TAG(tu.all);
		void* p=(void*)GET_TYPES_PTR(tu.all);
		fwrite(&tag,sizeof(uint8_t),1,fp);
		switch(tag)
		{
			case NATIVE_TYPE_TAG:
				fwrite(&((VMNativeType*)p)->type,sizeof(((VMNativeType*)p)->type),1,fp);
				fwrite(&((VMNativeType*)p)->size,sizeof(((VMNativeType*)p)->size),1,fp);
				break;
			case ARRAY_TYPE_TAG:
				fwrite(&((VMArrayType*)p)->etype,sizeof(((VMArrayType*)p)->etype),1,fp);
				fwrite(&((VMArrayType*)p)->num,sizeof(((VMArrayType*)p)->num),1,fp);
				break;
			case PTR_TYPE_TAG:
				fwrite(&((VMPtrType*)p)->ptype,sizeof(((VMPtrType*)p)->ptype),1,fp);
				break;
			case STRUCT_TYPE_TAG:
				{
					uint32_t num=((VMStructType*)p)->num;
					fwrite(&num,sizeof(num),1,fp);
					fwrite(&((VMStructType*)p)->totalSize,sizeof(((VMStructType*)p)->totalSize),1,fp);
					VMStructMember* members=((VMStructType*)p)->layout;
					fwrite(members,sizeof(VMStructMember),num,fp);
				}
				break;
			case UNION_TYPE_TAG:
				{
					uint32_t num=((VMUnionType*)p)->num;
					fwrite(&num,sizeof(num),1,fp);
					fwrite(&((VMUnionType*)p)->maxSize,sizeof(((VMUnionType*)p)->maxSize),1,fp);
					VMStructMember* members=((VMUnionType*)p)->layout;
					fwrite(members,sizeof(VMStructMember),num,fp);
				}
				break;
			case FUNC_TYPE_TAG:
				{
					VMFuncType* ft=(VMFuncType*)p;
					fwrite(&ft->rtype,sizeof(ft->rtype),1,fp);
					uint32_t anum=ft->anum;
					fwrite(&anum,sizeof(anum),1,fp);
					TypeId_t* atypes=ft->atypes;
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
	TypeId_t i=0;
	TypeId_t num=0;
	VMTypeUnion* ul=NULL;
	fread(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fread(&CharTypeId,sizeof(CharTypeId),1,fp);
	fread(&num,sizeof(num),1,fp);
	ul=(VMTypeUnion*)malloc(sizeof(VMTypeUnion)*num);
	FAKE_ASSERT(ul,"fklLoadTypeList",__FILE__,__LINE__);
	for(;i<num;i++)
	{
		DefTypeTag tag=NATIVE_TYPE_TAG;
		fread(&tag,sizeof(uint8_t),1,fp);
		VMTypeUnion tu={.all=NULL};
		switch(tag)
		{
			case NATIVE_TYPE_TAG:
				{
					VMNativeType* t=(VMNativeType*)malloc(sizeof(VMNativeType));
					FAKE_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					fread(&t->type,sizeof(t->type),1,fp);
					fread(&t->size,sizeof(t->size),1,fp);
					tu.nt=(VMNativeType*)MAKE_NATIVE_TYPE(t);
				}
				break;
			case ARRAY_TYPE_TAG:
				{
					VMArrayType* t=(VMArrayType*)malloc(sizeof(VMArrayType));
					FAKE_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					fread(&t->etype,sizeof(t->etype),1,fp);
					fread(&t->num,sizeof(t->num),1,fp);
					tu.at=(VMArrayType*)MAKE_ARRAY_TYPE(t);
				}
				break;
			case PTR_TYPE_TAG:
				{
					VMPtrType* t=(VMPtrType*)malloc(sizeof(VMPtrType));
					FAKE_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					fread(&t->ptype,sizeof(t->ptype),1,fp);
					tu.pt=(VMPtrType*)MAKE_PTR_TYPE(t);
				}
				break;
			case STRUCT_TYPE_TAG:
				{
					uint32_t num=0;
					fread(&num,sizeof(num),1,fp);
					VMStructType* t=(VMStructType*)malloc(sizeof(VMStructType)+sizeof(VMStructMember)*num);
					FAKE_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					t->num=num;
					fread(&t->totalSize,sizeof(t->totalSize),1,fp);
					fread(t->layout,sizeof(VMStructMember),num,fp);
					tu.st=(VMStructType*)MAKE_STRUCT_TYPE(t);
				}
				break;
			case UNION_TYPE_TAG:
				{
					uint32_t num=0;
					fread(&num,sizeof(num),1,fp);
					VMUnionType* t=(VMUnionType*)malloc(sizeof(VMUnionType)+sizeof(VMStructMember)*num);
					FAKE_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					t->num=num;
					fread(&t->maxSize,sizeof(t->maxSize),1,fp);
					fread(t->layout,sizeof(VMStructMember),num,fp);
					tu.ut=(VMUnionType*)MAKE_UNION_TYPE(t);
				}
			case FUNC_TYPE_TAG:
				{
					TypeId_t rtype=0;
					fread(&rtype,sizeof(rtype),1,fp);
					uint32_t anum=0;
					fread(&anum,sizeof(anum),1,fp);
					VMFuncType* t=(VMFuncType*)malloc(sizeof(VMUnionType)+sizeof(TypeId_t)*anum);
					FAKE_ASSERT(t,"fklLoadTypeList",__FILE__,__LINE__);
					t->rtype=rtype;
					t->anum=anum;
					fread(t->atypes,sizeof(TypeId_t),anum,fp);
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
	TypeId_t num=GlobTypeUnionList.num;
	VMTypeUnion* ul=GlobTypeUnionList.ul;
	TypeId_t i=0;
	for(;i<num;i++)
	{
		VMTypeUnion tu=ul[i];
		DefTypeTag tag=GET_TYPES_TAG(tu.all);
		switch(tag)
		{
			case NATIVE_TYPE_TAG:
				fklFreeVMNativeType((VMNativeType*)GET_TYPES_PTR(tu.all));
				break;
			case PTR_TYPE_TAG:
				fklFreeVMPtrType((VMPtrType*)GET_TYPES_PTR(tu.all));
				break;
			case ARRAY_TYPE_TAG:
				fklFreeVMArrayType((VMArrayType*)GET_TYPES_PTR(tu.all));
				break;
			case STRUCT_TYPE_TAG:
				fklFreeVMStructType((VMStructType*)GET_TYPES_PTR(tu.all));
				break;
			case UNION_TYPE_TAG:
				fklFreeVMUnionType((VMUnionType*)GET_TYPES_PTR(tu.all));
				break;
			case FUNC_TYPE_TAG:
				fklFreeVMFuncType((VMFuncType*)GET_TYPES_PTR(tu.all));
				break;
			default:
				break;
		}
	}
	free(ul);
}

int fklIsNativeTypeId(TypeId_t type)
{
	VMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==NATIVE_TYPE_TAG;
}

int fklIsArrayTypeId(TypeId_t type)
{
	VMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==ARRAY_TYPE_TAG;
}

int fklIsPtrTypeId(TypeId_t type)
{
	VMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==PTR_TYPE_TAG;
}

int fklIsStructTypeId(TypeId_t type)
{
	VMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==STRUCT_TYPE_TAG;
}

int fklIsUnionTypeId(TypeId_t type)
{
	VMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==UNION_TYPE_TAG;
}

int fklIsFunctionTypeId(TypeId_t type)
{
	VMTypeUnion tu=fklGetVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FUNC_TYPE_TAG;
}
