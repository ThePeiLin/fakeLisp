#include"VMtool.h"
#include"common.h"
#include"opcode.h"
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
#define CAST_TO_IN64(TYPE) int64_t t=*(TYPE*)mem;return newVMvalue(IN64,&t,heap);
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
static VMvalue* castFloat    (ARGL){double t=*(float*)mem;return newVMvalue(DBL,&t,heap);}
static VMvalue* castDouble   (ARGL){double t=*(double*)mem;return newVMvalue(DBL,&t,heap);}
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
static VMvalue* castVPtr     (ARGL){return newVMvalue(IN64,mem,heap);}
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
	for(uint32_t i=0;i<num;totalSize+=getVMTypeSize(getVMTypeUnion(memberTypes[i])),i++);
	VMTypeUnion* pst=&GlobTypeUnionList.ul[id-1];
	VMStructType* ost=(VMStructType*)GET_TYPES_PTR(pst->st);
	ost=(VMStructType*)realloc(ost,sizeof(VMStructType)+num*sizeof(VMStructMember));
	FAKE_ASSERT(ost,"initVMStructTypeId",__FILE__,__LINE__);
	if(structName)
		ost->type=addSymbolToGlob(structName)->id;
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
		size_t curSize=getVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
			maxSize=curSize;
	}
	VMTypeUnion* put=&GlobTypeUnionList.ul[id-1];
	VMUnionType* out=(VMUnionType*)GET_TYPES_PTR(put->ut);
	out=(VMUnionType*)realloc(out,sizeof(VMUnionType)+num*sizeof(VMStructMember));
	FAKE_ASSERT(out,"initVMUnionTypeId",__FILE__,__LINE__);
	if(unionName)
		out->type=addSymbolToGlob(unionName)->id;
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
	AST_cptr* numCptr=nextCptr(compositeDataHead);
	if(!numCptr||numCptr->type!=ATM||numCptr->u.atom->type!=IN32)
		return 0;
	AST_cptr* typeCptr=nextCptr(numCptr);
	if(!typeCptr)
		return 0;
	TypeId_t type=genDefTypesUnion(typeCptr,otherTypes);
	if(!type)
		return type;
	if(getCptrCdr(typeCptr)->type!=NIL)
		return 0;
	return newVMArrayType(type,numCptr->u.atom->value.in32);
}

static TypeId_t genPtrTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	AST_cptr* ptrTypeCptr=nextCptr(compositeDataHead);
	if(ptrTypeCptr->type==ATM&&ptrTypeCptr->u.atom->type!=SYM)
		return 0;
	TypeId_t pType=genDefTypesUnion(ptrTypeCptr,otherTypes);
	if(!pType)
		return pType;
	if(getCptrCdr(ptrTypeCptr)->type!=NIL)
		return 0;
	return newVMPtrType(pType);
}

static int isInPtrDeclare(AST_cptr* compositeDataHead)
{
	AST_pair* outerPair=compositeDataHead->outer;
	AST_pair* outerPrevPair=outerPair->prev;
	if(outerPrevPair)
	{
		AST_cptr* outerTypeHead=prevCptr(&outerPrevPair->car);
		if(outerTypeHead)
			return outerTypeHead->type==ATM&&outerTypeHead->u.atom->type==SYM&&!strcmp(outerTypeHead->u.atom->value.str,"ptr");
	}
	return 0;
}

static TypeId_t genStructTypeId(AST_cptr* compositeDataHead,VMDefTypes* otherTypes)
{
	char* structName=NULL;
	AST_cptr* structNameCptr=nextCptr(compositeDataHead);
	uint32_t num=0;
	AST_cptr* memberCptr=NULL;
	if(structNameCptr->type==ATM)
	{
		if(structNameCptr->u.atom->type==SYM)
			structName=structNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=nextCptr(structNameCptr);
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
				if(st->type==addSymbolToGlob(structName)->id)
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
		retval=newVMStructType(structName,0,NULL,NULL);
	for(;memberCptr;num++,memberCptr=nextCptr(memberCptr));
	memberCptr=structNameCptr;
	Sid_t* memberSymbolList=NULL;
	TypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(Sid_t*)malloc(sizeof(Sid_t)*num);
		memberTypeList=(TypeId_t*)malloc(sizeof(TypeId_t)*num);
		FAKE_ASSERT(memberTypeList&&memberCptr,"genDefTypesUnion",__FILE__,__LINE__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=nextCptr(memberCptr))
		{
			AST_cptr* cdr=getCptrCdr(memberCptr);
			if(cdr->type!=PAIR&&cdr->type!=NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			AST_cptr* memberName=getFirstCptr(memberCptr);
			if(memberName->type!=ATM||memberName->u.atom->type!=SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			Sid_t symbol=addSymbolToGlob(memberName->u.atom->value.str)->id;
			if(nextCptr(nextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			TypeId_t type=genDefTypesUnion(nextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(getCptrCdr(nextCptr(memberName))->type!=NIL)
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
		retval=newVMStructType(structName,num,memberSymbolList,memberTypeList);
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
	AST_cptr* unionNameCptr=nextCptr(compositeDataHead);
	uint32_t num=0;
	AST_cptr* memberCptr=NULL;
	if(unionNameCptr->type==ATM)
	{
		if(unionNameCptr->u.atom->type==SYM)
			unionName=unionNameCptr->u.atom->value.str;
		else
			return 0;
		memberCptr=nextCptr(unionNameCptr);
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
				if(ut->type==addSymbolToGlob(unionName)->id)
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
		retval=newVMUnionType(unionName,0,NULL,NULL);
	for(;memberCptr;num++,memberCptr=nextCptr(memberCptr));
	memberCptr=unionNameCptr;
	Sid_t* memberSymbolList=NULL;
	TypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(Sid_t*)malloc(sizeof(Sid_t)*num);
		memberTypeList=(TypeId_t*)malloc(sizeof(TypeId_t)*num);
		FAKE_ASSERT(memberTypeList&&memberCptr,"genDefTypesUnion",__FILE__,__LINE__);
		uint32_t i=0;
		for(;memberCptr;i++,memberCptr=nextCptr(memberCptr))
		{
			AST_cptr* cdr=getCptrCdr(memberCptr);
			if(cdr->type!=PAIR&&cdr->type!=NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			AST_cptr* memberName=getFirstCptr(memberCptr);
			if(memberName->type!=ATM||memberName->u.atom->type!=SYM)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			Sid_t symbol=addSymbolToGlob(memberName->u.atom->value.str)->id;
			if(nextCptr(nextCptr(memberName)))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			TypeId_t type=genDefTypesUnion(nextCptr(memberName),otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			if(getCptrCdr(nextCptr(memberName))->type!=NIL)
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
		retval=newVMUnionType(unionName,num,memberSymbolList,memberTypeList);
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
	AST_cptr* argCptr=nextCptr(compositeDataHead);
	if(!argCptr||(argCptr->type!=PAIR&&argCptr->type!=NIL))
		return 0;
	AST_cptr* rtypeCptr=nextCptr(argCptr);
	if(rtypeCptr)
	{
		TypeId_t tmp=genDefTypesUnion(rtypeCptr,otherTypes);
		if(!tmp)
			return 0;
		VMTypeUnion tu=getVMTypeUnion(tmp);
		if(GET_TYPES_TAG(tu.all)!=NATIVE_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=PTR_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=ARRAY_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FUNC_TYPE_TAG)
			return 0;
		rtype=tmp;
	}
	uint32_t i=0;
	AST_cptr* firArgCptr=getFirstCptr(argCptr);
	for(;firArgCptr;firArgCptr=nextCptr(firArgCptr),i++);
	TypeId_t* atypes=(TypeId_t*)malloc(sizeof(TypeId_t)*i);
	FAKE_ASSERT(atypes,"genDefTypesUnion",__FILE__,__LINE__);
	for(i=0,firArgCptr=getFirstCptr(argCptr);firArgCptr;firArgCptr=nextCptr(firArgCptr),i++)
	{
		TypeId_t tmp=genDefTypesUnion(firArgCptr,otherTypes);
		if(!tmp)
		{
			free(atypes);
			return 0;
		}
		VMTypeUnion tu=getVMTypeUnion(tmp);
		if(GET_TYPES_TAG(tu.all)!=NATIVE_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=PTR_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=ARRAY_TYPE_TAG&&GET_TYPES_TAG(tu.all)!=FUNC_TYPE_TAG)
		{
			free(atypes);
			return 0;
		}
		AST_cptr* cdr=getCptrCdr(firArgCptr);
		if(!tmp||(cdr->type!=PAIR&&cdr->type!=NIL))
		{
			free(atypes);
			return 0;
		}
		atypes[i]=tmp;
	}
	TypeId_t retval=newVMFuncType(rtype,i,atypes);
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

VMproc* newVMproc(uint32_t scp,uint32_t cpc)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	FAKE_ASSERT(tmp,"newVMproc",__FILE__,__LINE__);
	tmp->prevEnv=NULL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	return tmp;
}

VMvalue* copyVMvalue(VMvalue* obj,VMheap* heap)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	VMvalue* tmp=VM_NIL;
	pushComStack(obj,s1);
	pushComStack(&tmp,s2);
	while(!isComStackEmpty(s1))
	{
		VMvalue* root=popComStack(s1);
		VMptrTag tag=GET_TAG(root);
		VMvalue** root1=popComStack(s2);
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
							*root1=newVMvalue(DBL,root->u.dbl,heap);
							break;
						case BYTS:
							*root1=newVMvalue(BYTS,newVMByts(root->u.byts->size,root->u.byts->str),heap);
							break;
						case STR:
							*root1=newVMvalue(STR,copyStr(root->u.str),heap);
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
								VMChanl* tmpCh=newVMChanl(objCh->max);
								QueueNode* cur=objCh->messages->head;
								for(;cur;cur=cur->next)
								{
									void* tmp=VM_NIL;
									pushComQueue(tmp,tmpCh->messages);
									pushComStack(cur->data,s1);
									pushComStack(tmp,s2);
								}
								*root1=newVMvalue(CHAN,tmpCh,heap);
							}
							break;
						case PAIR:
							*root1=newVMvalue(PAIR,newVMpair(),heap);
							pushComStack(&(*root1)->u.pair->car,s2);
							pushComStack(&(*root1)->u.pair->cdr,s2);
							pushComStack(root->u.pair->car,s1);
							pushComStack(root->u.pair->cdr,s1);
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
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

VMvalue* newVMvalue(ValueType type,void* pValue,VMheap* heap)
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
				FAKE_ASSERT(tmp,"newVMvalue",__FILE__,__LINE__);
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
						tmp->u.dbl=copyMemory(pValue,sizeof(double));
						break;
					case IN64:
						tmp->u.in64=copyMemory(pValue,sizeof(int64_t));
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

VMvalue* newTrueValue(VMheap* heap)
{
	VMvalue* tmp=newVMvalue(IN32,(VMptr)1,heap);
	return tmp;
}

VMvalue* newNilValue(VMheap* heap)
{
	VMvalue* tmp=newVMvalue(NIL,NULL,heap);
	return tmp;
}

VMvalue* getTopValue(VMstack* stack)
{
	return stack->values[stack->tp-1];
}

VMvalue* getValue(VMstack* stack,int32_t place)
{
	return stack->values[place];
}

VMvalue* getVMpairCar(VMvalue* obj)
{
	return obj->u.pair->car;
}

VMvalue* getVMpairCdr(VMvalue* obj)
{
	return obj->u.pair->cdr;
}

int VMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	pushComStack(fir,s1);
	pushComStack(sec,s2);
	int r=1;
	while(!isComStackEmpty(s1))
	{
		VMvalue* root1=popComStack(s1);
		VMvalue* root2=popComStack(s2);
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
					r=eqVMByts(root1->u.byts,root2->u.byts);
					break;
				case PAIR:
					r=1;
					pushComStack(root1->u.pair->car,s1);
					pushComStack(root1->u.pair->cdr,s1);
					pushComStack(root2->u.pair->car,s2);
					pushComStack(root2->u.pair->cdr,s2);
					break;
				default:
					r=(root1==root2);
					break;
			}
		}
		if(!r)
			break;
	}
	freeComStack(s1);
	freeComStack(s2);
	return r;
}

int subVMvaluecmp(VMvalue* fir,VMvalue* sec)
{
	return fir==sec;
}

int numcmp(VMvalue* fir,VMvalue* sec)
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

VMenv* newVMenv(VMenv* prev)
{
	VMenv* tmp=(VMenv*)malloc(sizeof(VMenv));
	FAKE_ASSERT(tmp,"newVMenv",__FILE__,__LINE__);
	tmp->num=0;
	tmp->list=NULL;
	tmp->prev=prev;
	increaseVMenvRefcount(prev);
	tmp->refcount=0;
	return tmp;
}

void increaseVMenvRefcount(VMenv* env)
{
	INCREASE_REFCOUNT(VMenv,env);
}

void decreaseVMenvRefcount(VMenv* env)
{
	DECREASE_REFCOUNT(VMenv,env);
}

VMpair* newVMpair(void)
{
	VMpair* tmp=(VMpair*)malloc(sizeof(VMpair));
	FAKE_ASSERT(tmp,"newVMpair",__FILE__,__LINE__);
	tmp->car=VM_NIL;
	tmp->cdr=VM_NIL;
	return tmp;
}

VMvalue* castCptrVMvalue(AST_cptr* objCptr,VMheap* heap)
{
	ComStack* s1=newComStack(32);
	ComStack* s2=newComStack(32);
	VMvalue* tmp=VM_NIL;
	pushComStack(objCptr,s1);
	pushComStack(&tmp,s2);
	while(!isComStackEmpty(s1))
	{
		AST_cptr* root=popComStack(s1);
		VMvalue** root1=popComStack(s2);
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
					*root1=MAKE_VM_SYM(addSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case DBL:
					*root1=newVMvalue(DBL,&tmpAtm->value.dbl,heap);
					break;
				case BYTS:
					*root1=newVMvalue(BYTS,newVMByts(tmpAtm->value.byts.size,tmpAtm->value.byts.str),heap);
					break;
				case STR:
					*root1=newVMvalue(STR,copyStr(tmpAtm->value.str),heap);
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==PAIR)
		{
			AST_pair* objPair=root->u.pair;
			VMpair* tmpPair=newVMpair();
			*root1=newVMvalue(PAIR,tmpPair,heap);
			pushComStack(&objPair->car,s1);
			pushComStack(&objPair->cdr,s1);
			pushComStack(&tmpPair->car,s2);
			pushComStack(&tmpPair->cdr,s2);
			*root1=MAKE_VM_PTR(*root1);
		}
	}
	freeComStack(s1);
	freeComStack(s2);
	return tmp;
}

VMByts* newVMByts(size_t size,uint8_t* str)
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts)+size);
	FAKE_ASSERT(tmp,"newVMByts",__FILE__,__LINE__);
	tmp->size=size;
	memcpy(tmp->str,str,size);
	return tmp;
}

VMByts* copyVMByts(const VMByts* obj)
{
	if(obj==NULL)return NULL;
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts)+obj->size);
	FAKE_ASSERT(tmp,"copyVMByts",__FILE__,__LINE__);
	memcpy(tmp->str,obj->str,obj->size);
	tmp->size=obj->size;
	return tmp;
}

void VMBytsCat(VMByts** fir,const VMByts* sec)
{
	size_t firSize=(*fir)->size;
	size_t secSize=sec->size;
	*fir=(VMByts*)realloc(*fir,sizeof(VMByts)+(firSize+secSize)*sizeof(uint8_t));
	FAKE_ASSERT(*fir,"VMBytsCat",__FILE__,__LINE__);
	(*fir)->size=firSize+secSize;
	memcpy((*fir)->str+firSize,sec->str,secSize);
}
VMByts* newEmptyVMByts()
{
	VMByts* tmp=(VMByts*)malloc(sizeof(VMByts));
	FAKE_ASSERT(tmp,"newEmptyVMByts",__FILE__,__LINE__);
	tmp->size=0;
	return tmp;
}

uint8_t* copyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FAKE_ASSERT(tmp,"copyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

VMproc* copyVMproc(VMproc* obj,VMheap* heap)
{
	VMproc* tmp=(VMproc*)malloc(sizeof(VMproc));
	FAKE_ASSERT(tmp,"copyVMproc",__FILE__,__LINE__);
	tmp->scp=obj->scp;
	tmp->cpc=obj->cpc;
	tmp->prevEnv=copyVMenv(obj->prevEnv,heap);
	return tmp;
}

VMenv* copyVMenv(VMenv* objEnv,VMheap* heap)
{
	VMenv* tmp=newVMenv(NULL);
	tmp->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*objEnv->num);
	FAKE_ASSERT(tmp->list,"copyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->num;i++)
	{
		VMenvNode* node=objEnv->list[i];
		VMvalue* v=copyVMvalue(node->value,heap);
		VMenvNode* tmp=newVMenvNode(v,node->id);
		objEnv->list[i]=tmp;
	}
	tmp->prev=(objEnv->prev->refcount==0)?copyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void freeVMproc(VMproc* proc)
{
	if(proc->prevEnv)
		freeVMenv(proc->prevEnv);
	free(proc);
}

void freeVMenv(VMenv* obj)
{
	while(obj!=NULL)
	{
		if(!obj->refcount)
		{
			VMenv* prev=obj;
			obj=obj->prev;
			int32_t i=0;
			for(;i<prev->num;i++)
				freeVMenvNode(prev->list[i]);
			free(prev->list);
			free(prev);
		}
		else
		{
			decreaseVMenvRefcount(obj);
			break;
		}
	}
}

void releaseSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_unlock(pGClock);
}

void lockSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_rdlock(pGClock);
}

VMvalue* popVMstack(VMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	VMvalue* tmp=getTopValue(stack);
	stack->tp-=1;
	return tmp;
}

VMenv* castPreEnvToVMenv(PreEnv* pe,VMenv* prev,VMheap* heap)
{
	int32_t size=0;
	PreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	VMenv* tmp=newVMenv(prev);
	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
	{
		VMvalue* v=castCptrVMvalue(&tmpDef->obj,heap);
		VMenvNode* node=newVMenvNode(v,addSymbolToGlob(tmpDef->symbol)->id);
		addVMenvNode(node,tmp);
	}
	return tmp;
}

VMcontinuation* newVMcontinuation(VMstack* stack,ComStack* rstack,ComStack* tstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	FAKE_ASSERT(tmp,"newVMcontinuation",__FILE__,__LINE__);
	VMrunnable* status=(VMrunnable*)malloc(sizeof(VMrunnable)*size);
	FAKE_ASSERT(status,"newVMcontinuation",__FILE__,__LINE__);
	int32_t tbnum=tstack->top;
	VMTryBlock* tb=(VMTryBlock*)malloc(sizeof(VMTryBlock)*tbnum);
	FAKE_ASSERT(tb,"newVMcontinuation",__FILE__,__LINE__);
	tmp->stack=copyStack(stack);
	tmp->num=size;
	for(;i<size;i++)
	{
		VMrunnable* cur=rstack->data[i];
		status[i].cp=cur->cp;
		increaseVMenvRefcount(cur->localenv);
		status[i].localenv=cur->localenv;
		status[i].cpc=cur->cpc;
		status[i].scp=cur->scp;
		status[i].mark=cur->mark;
	}
	tmp->tnum=tbnum;
	for(i=0;i<tbnum;i++)
	{
		VMTryBlock* cur=tstack->data[i];
		tb[i].sid=cur->sid;
		ComStack* hstack=cur->hstack;
		int32_t handlerNum=hstack->top;
		ComStack* curHstack=newComStack(handlerNum);
		int32_t j=0;
		for(;j<handlerNum;j++)
		{
			VMerrorHandler* curH=hstack->data[i];
			VMerrorHandler* h=newVMerrorHandler(curH->type,curH->proc.scp,curH->proc.cpc);
			pushComStack(h,curHstack);
		}
		tb[i].hstack=curHstack;
		tb[i].rtp=cur->rtp;
		tb[i].tp=cur->tp;
	}
	tmp->status=status;
	tmp->tb=tb;
	return tmp;
}

void freeVMcontinuation(VMcontinuation* cont)
{
	int32_t i=0;
	int32_t size=cont->num;
	int32_t tbsize=cont->tnum;
	VMstack* stack=cont->stack;
	VMrunnable* status=cont->status;
	VMTryBlock* tb=cont->tb;
	free(stack->tpst);
	free(stack->values);
	free(stack);
	for(;i<size;i++)
		freeVMenv(status[i].localenv);
	for(i=0;i<tbsize;i++)
	{
		ComStack* hstack=tb[i].hstack;
		while(!isComStackEmpty(hstack))
		{
			VMerrorHandler* h=popComStack(hstack);
			freeVMerrorHandler(h);
		}
		freeComStack(hstack);
	}
	free(tb);
	free(status);
	free(cont);
}

VMstack* copyStack(VMstack* stack)
{
	int32_t i=0;
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	FAKE_ASSERT(tmp,"copyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(VMvalue**)malloc(sizeof(VMvalue*)*(tmp->size));
	FAKE_ASSERT(tmp->values,"copyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FAKE_ASSERT(tmp->tpst,"copyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

VMenvNode* newVMenvNode(VMvalue* value,int32_t id)
{
	VMenvNode* tmp=(VMenvNode*)malloc(sizeof(VMenvNode));
	FAKE_ASSERT(tmp,"newVMenvNode",__FILE__,__LINE__);
	tmp->id=id;
	tmp->value=value;
	return tmp;
}

VMenvNode* addVMenvNode(VMenvNode* node,VMenv* env)
{
	if(!env->list)
	{
		env->num=1;
		env->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*1);
		FAKE_ASSERT(env->list,"addVMenvNode",__FILE__,__LINE__);
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
		FAKE_ASSERT(env->list,"addVMenvNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		env->list[mid]=node;
	}
	return node;
}

VMenvNode* findVMenvNode(Sid_t id,VMenv* env)
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

void freeVMenvNode(VMenvNode* node)
{
	free(node);
}


VMChanl* newVMChanl(int32_t maxSize)
{
	VMChanl* tmp=(VMChanl*)malloc(sizeof(VMChanl));
	FAKE_ASSERT(tmp,"newVMChanl",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->messages=newComQueue();
	tmp->sendq=newComQueue();
	tmp->recvq=newComQueue();
	return tmp;
}

int32_t getNumVMChanl(VMChanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=lengthComQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

void freeVMChanl(VMChanl* ch)
{
	pthread_mutex_destroy(&ch->lock);
	freeComQueue(ch->messages);
	QueueNode* head=ch->sendq->head;
	for(;head;head=head->next)
		freeSendT(head->data);
	for(head=ch->recvq->head;head;head=head->next)
		freeRecvT(head->data);
	freeComQueue(ch->sendq);
	freeComQueue(ch->recvq);
	free(ch);
}

VMChanl* copyVMChanl(VMChanl* ch,VMheap* heap)
{
	VMChanl* tmpCh=newVMChanl(ch->max);
	QueueNode* cur=ch->messages->head;
	ComQueue* tmp=newComQueue();
	for(;cur;cur=cur->next)
	{
		void* message=copyVMvalue(cur->data,heap);
		pushComQueue(message,tmp);
	}
	return tmpCh;
}

void freeVMfp(FILE* fp)
{
	if(fp!=stdin&&fp!=stdout&&fp!=stderr)
		fclose(fp);
}

DllHandle* newVMDll(const char* dllName)
{
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	size_t len=strlen(dllName)+strlen(filetype)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(realDllName,"newVMDll",__FILE__,__LINE__);
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
		free(tmp);
		return NULL;
	}
#else
	DllHandle handle=dlopen(rpath,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		free(rpath);
		free(realDllName);
		return NULL;
	}
#endif
	free(realDllName);
	free(rpath);
	return handle;
}

void freeVMDll(DllHandle* dll)
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

void* getAddress(const char* funcname,DllHandle dlhandle)
{
	void* pfunc=NULL;
#ifdef _WIN32
		pfunc=GetProcAddress(dlhandle,funcname);
#else
		pfunc=dlsym(dlhandle,funcname);
#endif
	return pfunc;
}

VMDlproc* newVMDlproc(DllFunc address,VMvalue* dll)
{
	VMDlproc* tmp=(VMDlproc*)malloc(sizeof(VMDlproc));
	FAKE_ASSERT(tmp,"newVMDlproc",__FILE__,__LINE__);
	tmp->func=address;
	tmp->dll=dll;
	return tmp;
}

void freeVMDlproc(VMDlproc* dlproc)
{
	free(dlproc);
}

VMFlproc* newVMFlproc(TypeId_t type,void* func,VMvalue* dll)
{
	VMFlproc* tmp=(VMFlproc*)malloc(sizeof(VMFlproc));
	FAKE_ASSERT(tmp,"newVMDlproc",__FILE__,__LINE__);
	tmp->type=type;
	tmp->func=func;
	tmp->dll=dll;
	return tmp;
}

void freeVMFlproc(VMFlproc* t)
{
	free(t);
}

VMerror* newVMerror(const char* who,const char* type,const char* message)
{
	VMerror* t=(VMerror*)malloc(sizeof(VMerror));
	FAKE_ASSERT(t,"newVMerror",__FILE__,__LINE__);
	t->who=copyStr(who);
	t->type=addSymbolToGlob(type)->id;
	t->message=copyStr(message);
	return t;
}

VMerror* newVMerrorWithSid(const char* who,Sid_t type,const char* message)
{
	VMerror* t=(VMerror*)malloc(sizeof(VMerror));
	FAKE_ASSERT(t,"newVMerror",__FILE__,__LINE__);
	t->who=copyStr(who);
	t->type=type;
	t->message=copyStr(message);
	return t;
}

void freeVMerror(VMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

RecvT* newRecvT(FakeVM* v)
{
	RecvT* tmp=(RecvT*)malloc(sizeof(RecvT));
	FAKE_ASSERT(tmp,"newRecvT",__FILE__,__LINE__);
	tmp->v=v;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void freeRecvT(RecvT* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

SendT* newSendT(VMvalue* m)
{
	SendT* tmp=(SendT*)malloc(sizeof(SendT));
	FAKE_ASSERT(tmp,"newSendT",__FILE__,__LINE__);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void freeSendT(SendT* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

void chanlRecv(RecvT* r,VMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(!lengthComQueue(ch->messages))
	{
		pushComQueue(r,ch->recvq);
		pthread_cond_wait(&r->cond,&ch->lock);
	}
	SET_RETURN("chanlRecv",popComQueue(ch->messages),r->v->stack);
	if(lengthComQueue(ch->messages)<ch->max)
	{
		SendT* s=popComQueue(ch->sendq);
		if(s)
			pthread_cond_signal(&s->cond);
	}
	freeRecvT(r);
	pthread_mutex_unlock(&ch->lock);
}

void chanlSend(SendT*s,VMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(lengthComQueue(ch->recvq))
	{
		RecvT* r=popComQueue(ch->recvq);
		if(r)
			pthread_cond_signal(&r->cond);
	}
	if(!ch->max||lengthComQueue(ch->messages)<ch->max-1)
		pushComQueue(s->m,ch->messages);
	else
	{
		if(lengthComQueue(ch->messages)>=ch->max-1)
		{
			pushComQueue(s,ch->sendq);
			if(lengthComQueue(ch->messages)==ch->max-1)
				pushComQueue(s->m,ch->messages);
			pthread_cond_wait(&s->cond,&ch->lock);
		}
	}
	freeSendT(s);
	pthread_mutex_unlock(&ch->lock);
}

VMTryBlock* newVMTryBlock(Sid_t sid,uint32_t tp,long int rtp)
{
	VMTryBlock* t=(VMTryBlock*)malloc(sizeof(VMTryBlock));
	FAKE_ASSERT(t,"newVMTryBlock",__FILE__,__LINE__);
	t->sid=sid;
	t->hstack=newComStack(32);
	t->tp=tp;
	t->rtp=rtp;
	return t;
}

void freeVMTryBlock(VMTryBlock* b)
{
	ComStack* hstack=b->hstack;
	while(!isComStackEmpty(hstack))
	{
		VMerrorHandler* h=popComStack(hstack);
		freeVMerrorHandler(h);
	}
	freeComStack(b->hstack);
	free(b);
}

VMerrorHandler* newVMerrorHandler(Sid_t type,uint32_t scp,uint32_t cpc)
{
	VMerrorHandler* t=(VMerrorHandler*)malloc(sizeof(VMerrorHandler));
	FAKE_ASSERT(t,"newVMerrorHandler",__FILE__,__LINE__);
	t->type=type;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	return t;
}

void freeVMerrorHandler(VMerrorHandler* h)
{
	freeVMenv(h->proc.prevEnv);
	free(h);
}

int raiseVMerror(VMvalue* ev,FakeVM* exe)
{
	VMerror* err=ev->u.err;
	while(!isComStackEmpty(exe->tstack))
	{
		VMTryBlock* tb=topComStack(exe->tstack);
		VMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!isComStackEmpty(tb->hstack))
		{
			VMerrorHandler* h=popComStack(tb->hstack);
			if(h->type==err->type)
			{
				long int increment=exe->rstack->top-tb->rtp;
				unsigned int i=0;
				for(;i<increment;i++)
				{
					VMrunnable* runnable=popComStack(exe->rstack);
					freeVMenv(runnable->localenv);
					free(runnable);
				}
				VMrunnable* prevRunnable=topComStack(exe->rstack);
				VMrunnable* r=newVMrunnable(&h->proc);
				r->localenv=newVMenv(prevRunnable->localenv);
				VMenv* curEnv=r->localenv;
				Sid_t idOfError=tb->sid;
				VMenvNode* errorNode=findVMenvNode(idOfError,curEnv);
				if(!errorNode)
					errorNode=addVMenvNode(newVMenvNode(NULL,idOfError),curEnv);
				errorNode->value=ev;
				pushComStack(r,exe->rstack);
				freeVMerrorHandler(h);
				return 1;
			}
			freeVMerrorHandler(h);
		}
		freeVMTryBlock(popComStack(exe->tstack));
	}
	fprintf(stderr,"error of %s :%s",err->who,err->message);
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

VMrunnable* newVMrunnable(VMproc* code)
{
	VMrunnable* tmp=(VMrunnable*)malloc(sizeof(VMrunnable));
	FAKE_ASSERT(tmp,"newVMrunnable",__FILE__,__LINE__);
	if(code)
	{
		tmp->cp=code->scp;
		tmp->scp=code->scp;
		tmp->cpc=code->cpc;
	}
	tmp->mark=0;
	return tmp;
}

char* genErrorMessage(unsigned int type,VMrunnable* r,FakeVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=findLineNumTabNode(cp,exe->lnt);
	char* filename=GlobSymbolTable.idl[node->fid]->symbol;
	char* line=intToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(lineNumber,"genErrorMessage",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=copyStr("");
	switch(type)
	{
		case WRONGARG:
			t=strCat(t,"Wrong arguement ");
			break;
		case STACKERROR:
			t=strCat(t,"Stack error ");
			break;
		case TOOMANYARG:
			t=strCat(t,"Too many arguements ");
			break;
		case TOOFEWARG:
			t=strCat(t,"Too few arguements ");
			break;
		case CANTCREATETHREAD:
			t=strCat(t,"Can't create thread ");
			break;
		case SYMUNDEFINE:
			t=strCat(t,"Symbol ");
			t=strCat(t,GlobSymbolTable.idl[getSymbolIdInByteCode(exe->code+r->cp)]->symbol);
			t=strCat(t," is undefined ");
			break;
		case INVOKEERROR:
			t=strCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure ");
			break;
		case LOADDLLFAILD:
			t=strCat(t,"Faild to load dll \"");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=strCat(t,"\" ");
			break;
		case INVALIDSYMBOL:
			t=strCat(t,"Invalid symbol ");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=strCat(t," ");
			break;
		case DIVZERROERROR:
			t=strCat(t,"Divided by zero ");
			break;
		case FILEFAILURE:
			t=strCat(t,"Failed for file:\"");
			t=strCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=strCat(t,"\" ");
			break;
		case INVALIDASSIGN:
			t=strCat(t,"Invalid assign ");
			break;
		case INVALIDACCESS:
			t=strCat(t,"Invalid access ");
			break;
	}
	t=strCat(t,lineNumber);
	free(lineNumber);
	return t;
}

int32_t getSymbolIdInByteCode(const uint8_t* code)
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

int resBp(VMstack* stack)
{
	if(stack->tp>stack->bp)
		return TOOMANYARG;
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=GET_IN32(prevBp);
	stack->tp-=1;
	return 0;
}

void princVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
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
			fprintf(fp,"%s",getGlobSymbolWithId(GET_SYM(objValue))->symbol);
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
						for(;IS_PAIR(objValue);objValue=getVMpairCdr(objValue))
						{
							VMvalue* tmpValue=getVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								princVMvalue(tmpValue,fp,h);
							tmpValue=getVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								princVMvalue(tmpValue,fp,h);
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
						printByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,0);
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

void writeVMvalue(VMvalue* objValue,FILE* fp,CRL** h)
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
			printRawChar(GET_CHR(objValue),fp);
			break;
		case SYM_TAG:
			fprintf(fp,"%s",getGlobSymbolWithId(GET_SYM(objValue))->symbol);
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
						printRawString(objValue->u.str,fp);
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
						for(;IS_PAIR(objValue);objValue=getVMpairCdr(objValue))
						{
							VMvalue* tmpValue=getVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								writeVMvalue(tmpValue,fp,h);
							tmpValue=getVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								writeVMvalue(tmpValue,fp,h);
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
						printByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,1);
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
						fprintf(fp,"<#err w:%s t:%s m:%s>",objValue->u.err->who,getGlobSymbolWithId(objValue->u.err->type)->symbol,objValue->u.err->message);
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

int eqVMByts(const VMByts* fir,const VMByts* sec)
{
	if(fir->size!=sec->size)
		return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

VMvalue* GET_VAL(VMvalue* P,VMheap* heap)
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

int SET_REF(VMvalue* P,VMvalue* V)
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

TypeId_t newVMNativeType(Sid_t type,size_t size)
{
	VMNativeType* tmp=(VMNativeType*)malloc(sizeof(VMNativeType));
	FAKE_ASSERT(tmp,"newVMNativeType",__FILE__,__LINE__);
	tmp->type=type;
	tmp->size=size;
	return addToGlobTypeUnionList((VMTypeUnion)MAKE_NATIVE_TYPE(tmp));
}

void freeVMNativeType(VMNativeType* obj)
{
	free(obj);
}

VMTypeUnion getVMTypeUnion(TypeId_t t)
{
	return GlobTypeUnionList.ul[t-1];
}

size_t getVMTypeSizeWithTypeId(TypeId_t t)
{
	return getVMTypeSize(getVMTypeUnion(t));
}

size_t getVMTypeSize(VMTypeUnion t)
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

TypeId_t newVMArrayType(TypeId_t type,size_t num)
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
		FAKE_ASSERT(tmp,"newVMArrayType",__FILE__,__LINE__);
		tmp->etype=type;
		tmp->num=num;
		tmp->totalSize=num*getVMTypeSize(getVMTypeUnion(type));
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_ARRAY_TYPE(tmp));
	}
	else
		return id;
}

void freeVMArrayType(VMArrayType* obj)
{
	free(GET_TYPES_PTR(obj));
}

TypeId_t newVMPtrType(TypeId_t type)
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
		FAKE_ASSERT(tmp,"newVMPtrType",__FILE__,__LINE__);
		tmp->ptype=type;
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_PTR_TYPE(tmp));
	}
	return id;
}

void freeVMPtrType(VMPtrType* obj)
{
	free(GET_TYPES_PTR(obj));
}

TypeId_t newVMStructType(const char* structName,uint32_t num,Sid_t symbols[],TypeId_t memberTypes[])
{
	TypeId_t id=0;
	if(structName)
	{
		size_t i=0;
		size_t num=GlobTypeUnionList.num;
		Sid_t stype=addSymbolToGlob(structName)->id;
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
		for(uint32_t i=0;i<num;totalSize+=getVMTypeSize(getVMTypeUnion(memberTypes[i])),i++);
		VMStructType* tmp=(VMStructType*)malloc(sizeof(VMStructType)+sizeof(VMStructMember)*num);
		FAKE_ASSERT(tmp,"newVMStructType",__FILE__,__LINE__);
		if(structName)
			tmp->type=addSymbolToGlob(structName)->id;
		else
			tmp->type=-1;
		tmp->num=num;
		tmp->totalSize=totalSize;
		for(uint32_t i=0;i<num;i++)
		{
			tmp->layout[i].memberSymbol=symbols[i];
			tmp->layout[i].type=memberTypes[i];
		}
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_STRUCT_TYPE(tmp));
	}
	return id;
}

TypeId_t newVMUnionType(const char* unionName,uint32_t num,Sid_t symbols[],TypeId_t memberTypes[])
{
	size_t maxSize=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=getVMTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
			maxSize=curSize;
	}
	VMUnionType* tmp=(VMUnionType*)malloc(sizeof(VMUnionType)+sizeof(VMStructMember)*num);
	FAKE_ASSERT(tmp,"newVMStructType",__FILE__,__LINE__);
	if(unionName)
		tmp->type=addSymbolToGlob(unionName)->id;
	else
		tmp->type=-1;
	tmp->num=num;
	tmp->maxSize=maxSize;
	for(uint32_t i=0;i<num;i++)
	{
		tmp->layout[i].memberSymbol=symbols[i];
		tmp->layout[i].type=memberTypes[i];
	}
	return addToGlobTypeUnionList((VMTypeUnion)MAKE_UNION_TYPE(tmp));
}

void freeVMUnionType(VMUnionType* obj)
{
	free(GET_TYPES_PTR(obj));
}

void freeVMStructType(VMStructType* obj)
{
	free(GET_TYPES_PTR(obj));
}

TypeId_t newVMFuncType(TypeId_t rtype,uint32_t anum,TypeId_t atypes[])
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
		FAKE_ASSERT(tmp,"newVMFuncType",__FILE__,__LINE__);
		tmp->rtype=rtype;
		tmp->anum=anum;
		uint32_t i=0;
		for(;i<anum;i++)
			tmp->atypes[i]=atypes[i];
		return addToGlobTypeUnionList((VMTypeUnion)MAKE_FUNC_TYPE(tmp));
	}
	return id;
}

void freeVMFuncType(VMFuncType* obj)
{
	free(GET_TYPES_PTR(obj));
}

int addDefTypes(VMDefTypes* otherTypes,Sid_t typeName,TypeId_t type)
{
	if(otherTypes->num==0)
	{
		otherTypes->num+=1;
		VMDefTypesNode* node=(VMDefTypesNode*)malloc(sizeof(VMDefTypesNode));
		otherTypes->u=(VMDefTypesNode**)malloc(sizeof(VMDefTypesNode*)*1);
		FAKE_ASSERT(otherTypes->u&&node,"addDefTypes",__FILE__,__LINE__);
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
		FAKE_ASSERT(otherTypes->u,"addDefTypes",__FILE__,__LINE__);
		VMDefTypesNode* node=(VMDefTypesNode*)malloc(sizeof(VMDefTypesNode));
		FAKE_ASSERT(otherTypes->u&&node,"addDefTypes",__FILE__,__LINE__);
		node->name=typeName;
		node->type=type;
		for(;i>mid;i--)
			otherTypes->u[i]=otherTypes->u[i-1];
		otherTypes->u[mid]=node;
	}
	return 0;
}

VMDefTypesNode* findVMDefTypesNode(Sid_t typeName,VMDefTypes* otherTypes)
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

TypeId_t genDefTypes(AST_cptr* objCptr,VMDefTypes* otherTypes,Sid_t* typeName)
{
	AST_cptr* fir=nextCptr(getFirstCptr(objCptr));
	if(fir->type!=ATM||fir->u.atom->type!=SYM)
		return 0;
	Sid_t typeId=addSymbolToGlob(fir->u.atom->value.str)->id;
	if(isNativeType(typeId,otherTypes))
		return 0;
	*typeName=typeId;
	fir=nextCptr(fir);
	if(fir->type!=ATM&&fir->type!=PAIR)
		return 0;
	return genDefTypesUnion(fir,otherTypes);
}

TypeId_t genDefTypesUnion(AST_cptr* objCptr,VMDefTypes* otherTypes)
{
	if(objCptr->type==ATM&&objCptr->u.atom->type==SYM)
	{
		VMDefTypesNode* n=findVMDefTypesNode(addSymbolToGlob(objCptr->u.atom->value.str)->id,otherTypes);
		if(!n)
			return 0;
		else
			return n->type;
	}
	else if(objCptr->type==PAIR)
	{
		AST_cptr* compositeDataHead=getFirstCptr(objCptr);
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
	VMDefTypesNode* typeNode=findVMDefTypesNode(typeName,otherTypes);
	return typeNode!=NULL&&GET_TYPES_TAG(getVMTypeUnion(typeNode->type).all)==NATIVE_TYPE_TAG;
}

VMMem* newVMMem(TypeId_t type,uint8_t* mem)
{
	VMMem* tmp=(VMMem*)malloc(sizeof(VMMem));
	FAKE_ASSERT(tmp,"newVMMem",__FILE__,__LINE__);
	tmp->type=type;
	tmp->mem=mem;
	return tmp;
}

void initNativeDefTypes(VMDefTypes* otherTypes)
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
		Sid_t typeName=addSymbolToGlob(nativeTypeList[i].typeName)->id;
		size_t size=nativeTypeList[i].size;
		TypeId_t t=newVMNativeType(typeName,size);
		if(!CharTypeId&&!strcmp("char",nativeTypeList[i].typeName))
			CharTypeId=t;
		//VMTypeUnion t={.nt=newVMNativeType(typeName,size)};
		addDefTypes(otherTypes,typeName,t);
	}
	Sid_t otherTypeName=addSymbolToGlob("string")->id;
	StringTypeId=newVMNativeType(otherTypeName,sizeof(char*));
	addDefTypes(otherTypes,otherTypeName,StringTypeId);
	otherTypeName=addSymbolToGlob("FILE*")->id;
	FILEpTypeId=newVMNativeType(otherTypeName,sizeof(FILE*));
	addDefTypes(otherTypes,otherTypeName,FILEpTypeId);
	LastNativeTypeId=num;
	//i=0;
	//for(;i<num;i++)
	//	fprintf(stderr,"i=%ld typeId=%d\n",i,otherTypes->u[i]->name);
	//printGlobSymbolTable(stderr);
}

void writeTypeList(FILE* fp)
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

void loadTypeList(FILE* fp)
{
	TypeId_t i=0;
	TypeId_t num=0;
	VMTypeUnion* ul=NULL;
	fread(&LastNativeTypeId,sizeof(LastNativeTypeId),1,fp);
	fread(&CharTypeId,sizeof(CharTypeId),1,fp);
	fread(&num,sizeof(num),1,fp);
	ul=(VMTypeUnion*)malloc(sizeof(VMTypeUnion)*num);
	FAKE_ASSERT(ul,"loadTypeList",__FILE__,__LINE__);
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
					FAKE_ASSERT(t,"loadTypeList",__FILE__,__LINE__);
					fread(&t->type,sizeof(t->type),1,fp);
					fread(&t->size,sizeof(t->size),1,fp);
					tu.nt=(VMNativeType*)MAKE_NATIVE_TYPE(t);
				}
				break;
			case ARRAY_TYPE_TAG:
				{
					VMArrayType* t=(VMArrayType*)malloc(sizeof(VMArrayType));
					FAKE_ASSERT(t,"loadTypeList",__FILE__,__LINE__);
					fread(&t->etype,sizeof(t->etype),1,fp);
					fread(&t->num,sizeof(t->num),1,fp);
					tu.at=(VMArrayType*)MAKE_ARRAY_TYPE(t);
				}
				break;
			case PTR_TYPE_TAG:
				{
					VMPtrType* t=(VMPtrType*)malloc(sizeof(VMPtrType));
					FAKE_ASSERT(t,"loadTypeList",__FILE__,__LINE__);
					fread(&t->ptype,sizeof(t->ptype),1,fp);
					tu.pt=(VMPtrType*)MAKE_PTR_TYPE(t);
				}
				break;
			case STRUCT_TYPE_TAG:
				{
					uint32_t num=0;
					fread(&num,sizeof(num),1,fp);
					VMStructType* t=(VMStructType*)malloc(sizeof(VMStructType)+sizeof(VMStructMember)*num);
					FAKE_ASSERT(t,"loadTypeList",__FILE__,__LINE__);
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
					FAKE_ASSERT(t,"loadTypeList",__FILE__,__LINE__);
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
					FAKE_ASSERT(t,"loadTypeList",__FILE__,__LINE__);
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

void freeGlobTypeList()
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
				freeVMNativeType((VMNativeType*)GET_TYPES_PTR(tu.all));
				break;
			case PTR_TYPE_TAG:
				freeVMPtrType((VMPtrType*)GET_TYPES_PTR(tu.all));
				break;
			case ARRAY_TYPE_TAG:
				freeVMArrayType((VMArrayType*)GET_TYPES_PTR(tu.all));
				break;
			case STRUCT_TYPE_TAG:
				freeVMStructType((VMStructType*)GET_TYPES_PTR(tu.all));
				break;
			case UNION_TYPE_TAG:
				freeVMUnionType((VMUnionType*)GET_TYPES_PTR(tu.all));
				break;
			case FUNC_TYPE_TAG:
				freeVMFuncType((VMFuncType*)GET_TYPES_PTR(tu.all));
				break;
			default:
				break;
		}
	}
	free(ul);
}

int isNativeTypeId(TypeId_t type)
{
	VMTypeUnion tu=getVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==NATIVE_TYPE_TAG;
}

int isArrayTypeId(TypeId_t type)
{
	VMTypeUnion tu=getVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==ARRAY_TYPE_TAG;
}

int isPtrTypeId(TypeId_t type)
{
	VMTypeUnion tu=getVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==PTR_TYPE_TAG;
}

int isStructTypeId(TypeId_t type)
{
	VMTypeUnion tu=getVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==STRUCT_TYPE_TAG;
}

int isUnionTypeId(TypeId_t type)
{
	VMTypeUnion tu=getVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==UNION_TYPE_TAG;
}

int isFunctionTypeId(TypeId_t type)
{
	VMTypeUnion tu=getVMTypeUnion(type);
	return GET_TYPES_TAG(tu.all)==FUNC_TYPE_TAG;
}
