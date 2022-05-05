#include"fklffitype.h"
#include<fakeLisp/utils.h>
#include<string.h>

static pthread_rwlock_t GlobDefTypesLock=PTHREAD_RWLOCK_INITIALIZER;
static FklDefTypes* GlobDefTypes=NULL;

static FklTypeId_t LastNativeTypeId=0;
static FklTypeId_t I32TypeId=0;
static FklTypeId_t I64TypeId=0;
static FklTypeId_t F64TypeId=0;
static FklTypeId_t CharTypeId=0;
static FklTypeId_t StringTypeId=0;
static FklTypeId_t FILEpTypeId=0;

static FklSid_t ArrayTypedefSymbolId=0;
static FklSid_t PtrTypedefSymbolId=0;
static FklSid_t StructTypedefSymbolId=0;
static FklSid_t UnionTypedefSymbolId=0;
static FklSid_t FunctionTypedefSymbolId=0;

struct GlobTypeUnionListStruct
{
	FklTypeId_t num;
	FklDefTypeUnion* ul;
} GlobTypeUnionList={0,NULL};



void fklFfiInitGlobNativeTypes(void)
{
	GlobDefTypes=fklFfiNewDefTypes();
	fklFfiInitNativeDefTypes(GlobDefTypes);
}

void fklFfiFreeGlobDefTypeTable(void)
{
	pthread_rwlock_wrlock(&GlobDefTypesLock);
	fklFfiFreeDefTypeTable(GlobDefTypes);
	pthread_rwlock_unlock(&GlobDefTypesLock);
	pthread_rwlock_destroy(&GlobDefTypesLock);
}

void fklFfiInitNativeDefTypes(FklDefTypes* otherTypes)
{
	struct
	{
		char* typeName;
		size_t size;
	}nativeTypeList[]=
	{
		{"short",sizeof(short)},
		{"int",sizeof(int)},
		{"unsigned-short",sizeof(unsigned short)},
		{"unsigned",sizeof(unsigned)},
		{"long",sizeof(long)},
		{"unsigned-long",sizeof(unsigned long)},
		{"long-long",sizeof(long long)},
		{"unsigned-long-long",sizeof(unsigned long long)},
		{"ptrdiff_t",sizeof(ptrdiff_t)},
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
	size_t num=sizeof(nativeTypeList)/sizeof(nativeTypeList[0]);
	size_t i=0;
	for(;i<num;i++)
	{
		FklSid_t typeName=fklAddSymbolToGlob(nativeTypeList[i].typeName)->id;
		size_t size=nativeTypeList[i].size;
		FklTypeId_t t=fklFfiNewNativeType(typeName,size,size);
		fklFfiAddDefTypes(otherTypes,typeName,t);
	}
	FklSid_t otherTypeName=fklAddSymbolToGlob("string")->id;
	StringTypeId=fklFfiNewNativeType(otherTypeName,sizeof(char*),sizeof(char*));
	fklFfiAddDefTypes(otherTypes,otherTypeName,StringTypeId);
	otherTypeName=fklAddSymbolToGlob("FILE*")->id;
	FILEpTypeId=fklFfiNewNativeType(otherTypeName,sizeof(FILE*),sizeof(FILE*));
	fklFfiAddDefTypes(otherTypes,otherTypeName,FILEpTypeId);
	CharTypeId=12;
	I32TypeId=20;
	I64TypeId=22;
	F64TypeId=15;
	LastNativeTypeId=num;
}

static FklTypeId_t addToGlobTypeUnionList(FklDefTypeUnion type)
{
	GlobTypeUnionList.num+=1;
	size_t num=GlobTypeUnionList.num;
	GlobTypeUnionList.ul=(FklDefTypeUnion*)realloc(GlobTypeUnionList.ul,sizeof(FklDefTypeUnion)*num);
	FKL_ASSERT(GlobTypeUnionList.ul,__func__);
	GlobTypeUnionList.ul[num-1]=type;
	return num;
}

FklTypeId_t fklFfiNewStructType(FklSid_t structName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[])
{
	FklTypeId_t id=0;
	if(structName)
	{
		size_t i=0;
		size_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_STRUCT_TYPE_TAG)
			{
				FklDefStructType* structType=(FklDefTypeUnion){.st=FKL_GET_TYPES_PTR(tmpType.all)}.st;
				if(structType->type==structName)
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
			FklDefTypeUnion tu=fklFfiGetTypeUnion(memberTypes[i]);
			size_t align=fklFfiGetTypeAlign(tu);
			size_t size=fklFfiGetTypeSize(tu);
			totalSize+=(totalSize%align)?align-totalSize%align:0;
			totalSize+=size;
			if(maxSize<size)
			{
				maxSize=size;
				structAlign=align;
			}
		}
		totalSize+=(maxSize&&totalSize%maxSize)?maxSize-totalSize%maxSize:0;
		FklDefStructType* tmp=(FklDefStructType*)malloc(sizeof(FklDefStructType)+sizeof(FklDefStructMember)*num);
		FKL_ASSERT(tmp,__func__);
		tmp->type=structName;
		tmp->num=num;
		tmp->totalSize=totalSize;
		tmp->align=structAlign;
		for(uint32_t i=0;i<num;i++)
		{
			tmp->layout[i].memberSymbol=symbols[i];
			tmp->layout[i].type=memberTypes[i];
		}
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_STRUCT_TYPE(tmp));
	}
	return id;
}

FklTypeId_t fklFfiNewPtrType(FklTypeId_t type)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_PTR_TYPE_TAG)
		{
			FklDefPtrType* ptrType=(FklDefTypeUnion){.pt=FKL_GET_TYPES_PTR(tmpType.all)}.pt;
			if(ptrType->ptype==type)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklDefPtrType* tmp=(FklDefPtrType*)malloc(sizeof(FklDefPtrType));
		FKL_ASSERT(tmp,__func__);
		tmp->ptype=type;
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_PTR_TYPE(tmp));
	}
	return id;
}

FklTypeId_t fklFfiNewFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[])
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_FUNC_TYPE_TAG)
		{
			FklDefFuncType* funcType=(FklDefTypeUnion){.ft=FKL_GET_TYPES_PTR(tmpType.all)}.ft;
			if(funcType->rtype==rtype&&funcType->anum==anum&&!memcmp(funcType->atypes,atypes,sizeof(FklTypeId_t)*anum))
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklDefFuncType* tmp=(FklDefFuncType*)malloc(sizeof(FklDefFuncType)+sizeof(FklTypeId_t)*anum);
		FKL_ASSERT(tmp,__func__);
		tmp->rtype=rtype;
		tmp->anum=anum;
		uint32_t i=0;
		for(;i<anum;i++)
			tmp->atypes[i]=atypes[i];
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_FUNC_TYPE(tmp));
	}
	return id;
}

FklTypeId_t fklFfiNewArrayType(FklTypeId_t type,size_t num)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=GlobTypeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=GlobTypeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_DEF_ARRAY_TYPE_TAG)
		{
			FklDefArrayType* arrayType=(FklDefTypeUnion){.at=FKL_GET_TYPES_PTR(tmpType.all)}.at;
			if(arrayType->etype==type&&arrayType->num==num)
			{
				id=i+1;
				break;
			}
		}
	}
	if(!id)
	{
		FklDefArrayType* tmp=(FklDefArrayType*)malloc(sizeof(FklDefArrayType));
		FKL_ASSERT(tmp,__func__);
		tmp->etype=type;
		tmp->num=num;
		tmp->totalSize=num*fklFfiGetTypeSize(fklFfiGetTypeUnion(type));
		tmp->align=fklFfiGetTypeAlign(fklFfiGetTypeUnion(type));
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_ARRAY_TYPE(tmp));
	}
	else
		return id;
}

FklTypeId_t fklFfiNewUnionType(FklSid_t unionName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[])
{
	size_t maxSize=0;
	size_t align=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklFfiGetTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
		{
			maxSize=curSize;
			align=fklFfiGetTypeAlign(fklFfiGetTypeUnion(memberTypes[i]));
		}
	}
	FklDefUnionType* tmp=(FklDefUnionType*)malloc(sizeof(FklDefUnionType)+sizeof(FklDefStructMember)*num);
	FKL_ASSERT(tmp,__func__);
	tmp->type=unionName;
	tmp->num=num;
	tmp->maxSize=maxSize;
	tmp->align=align;
	for(uint32_t i=0;i<num;i++)
	{
		tmp->layout[i].memberSymbol=symbols[i];
		tmp->layout[i].type=memberTypes[i];
	}
	return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_UNION_TYPE(tmp));
}

FklDefTypes* fklFfiNewDefTypes(void)
{
	FklDefTypes* tmp=(FklDefTypes*)malloc(sizeof(FklDefTypes));
	FKL_ASSERT(tmp,__func__);
	tmp->num=0;
	tmp->u=NULL;
	return tmp;
}

FklTypeId_t fklFfiNewNativeType(FklSid_t type,size_t size,size_t align)
{
	FklDefNativeType* tmp=(FklDefNativeType*)malloc(sizeof(FklDefNativeType));
	FKL_ASSERT(tmp,__func__);
	tmp->type=type;
	tmp->align=align;
	tmp->size=size;
	return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_NATIVE_TYPE(tmp));
}

void fklFfiFreeNativeType(FklDefNativeType* obj)
{
	free(obj);
}

int fklFfiAddDefTypes(FklDefTypes* otherTypes,FklSid_t typeName,FklTypeId_t type)
{
	if(otherTypes->num==0)
	{
		otherTypes->num+=1;
		FklDefTypesNode* node=(FklDefTypesNode*)malloc(sizeof(FklDefTypesNode));
		otherTypes->u=(FklDefTypesNode**)malloc(sizeof(FklDefTypesNode*)*1);
		FKL_ASSERT(otherTypes->u&&node,__func__);
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
		otherTypes->u=(FklDefTypesNode**)realloc(otherTypes->u,sizeof(FklDefTypesNode*)*otherTypes->num);
		FKL_ASSERT(otherTypes->u,__func__);
		FklDefTypesNode* node=(FklDefTypesNode*)malloc(sizeof(FklDefTypesNode));
		FKL_ASSERT(otherTypes->u&&node,__func__);
		node->name=typeName;
		node->type=type;
		for(;i>mid;i--)
			otherTypes->u[i]=otherTypes->u[i-1];
		otherTypes->u[mid]=node;
	}
	return 0;
}

void fklFfiFreeDefTypeTable(FklDefTypes* defs)
{
	size_t i=0;
	size_t num=defs->num;
	for(;i<num;i++)
		free(defs->u[i]);
	free(defs->u);
	free(defs);
}

void fklFfiFreeGlobTypeList()
{
	FklTypeId_t num=GlobTypeUnionList.num;
	FklDefTypeUnion* ul=GlobTypeUnionList.ul;
	FklTypeId_t i=0;
	for(;i<num;i++)
	{
		FklDefTypeUnion tu=ul[i];
		FklDefTypeTag tag=FKL_GET_TYPES_TAG(tu.all);
		switch(tag)
		{
			case FKL_DEF_NATIVE_TYPE_TAG:
				fklFfiFreeNativeType(tu.all);
				break;
			case FKL_DEF_PTR_TYPE_TAG:
				fklFfiFreePtrType(tu.all);
				break;
			case FKL_DEF_ARRAY_TYPE_TAG:
				fklFfiFreeArrayType(tu.all);
				break;
			case FKL_DEF_STRUCT_TYPE_TAG:
				fklFfiFreeStructType(tu.all);
				break;
			case FKL_DEF_UNION_TYPE_TAG:
				fklFfiFreeUnionType(tu.all);
				break;
			case FKL_DEF_FUNC_TYPE_TAG:
				fklFfiFreeFuncType(tu.all);
				break;
			default:
				break;
		}
	}
	free(ul);
}

void fklFfiFreePtrType(FklDefPtrType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiFreeArrayType(FklDefArrayType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiFreeStructType(FklDefStructType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiFreeUnionType(FklDefUnionType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiFreeFuncType(FklDefFuncType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiInitTypedefSymbol(void)
{
	ArrayTypedefSymbolId=fklAddSymbolToGlob("array")->id;
	PtrTypedefSymbolId=fklAddSymbolToGlob("ptr")->id;
	StructTypedefSymbolId=fklAddSymbolToGlob("struct")->id;
	UnionTypedefSymbolId=fklAddSymbolToGlob("union")->id;
	FunctionTypedefSymbolId=fklAddSymbolToGlob("function")->id;
}

/*genTypeId functions list*/
static FklTypeId_t genArrayTypeId(FklVMvalue* numPair,FklDefTypes* otherTypes)
{
	FklVMvalue* numV=FKL_IS_PAIR(numPair)?numPair->u.pair->car:NULL;
	if(!numV||!fklIsInt(numV))
		return 0;
	FklVMvalue* typePair=numPair->u.pair->cdr;
	FklVMvalue* typeV=FKL_IS_PAIR(typePair)?typePair->u.pair->car:NULL;
	if(!typeV)
		return 0;
	FklTypeId_t type=fklFfiGenDefTypesUnion(typeV,otherTypes);
	if(!type)
		return type;
	if(typePair->u.pair->cdr!=FKL_VM_NIL)
		return 0;
	return fklFfiNewArrayType(type,fklGetInt(numV));
}

static FklTypeId_t genPtrTypeId(FklVMvalue* typePair,FklDefTypes* otherTypes)
{
	FklVMvalue* ptrTypeV=FKL_IS_PAIR(typePair)?typePair->u.pair->car:NULL;
	if(!ptrTypeV)
		return 0;
	FklTypeId_t pType=fklFfiGenDefTypesUnion(ptrTypeV,otherTypes);
	if(!pType)
		return pType;
	if(typePair->u.pair->cdr!=FKL_VM_NIL)
		return 0;
	return fklFfiNewPtrType(pType);
}

FklDefTypeUnion fklFfiGetTypeUnion(FklTypeId_t t)
{
	return GlobTypeUnionList.ul[t-1];
}

size_t fklFfiGetTypeSizeWithTypeId(FklTypeId_t t)
{
	return fklFfiGetTypeSize(fklFfiGetTypeUnion(t));
}

size_t fklFfiGetTypeSize(FklDefTypeUnion t)
{
	FklDefTypeTag tag=FKL_GET_TYPES_TAG(t.all);
	t.all=FKL_GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_DEF_NATIVE_TYPE_TAG:
			return t.nt->size;
			break;
		case FKL_DEF_ARRAY_TYPE_TAG:
			return t.at->totalSize;
			break;
		case FKL_DEF_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_DEF_STRUCT_TYPE_TAG:
			return t.st->totalSize;
			break;
		case FKL_DEF_UNION_TYPE_TAG:
			return t.ut->maxSize;
			break;
		case FKL_DEF_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

size_t fklFfiGetTypeAlign(FklDefTypeUnion t)
{
	FklDefTypeTag tag=FKL_GET_TYPES_TAG(t.all);
	t.all=FKL_GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_DEF_NATIVE_TYPE_TAG:
			return t.nt->align;
			break;
		case FKL_DEF_ARRAY_TYPE_TAG:
			return t.at->align;
			break;
		case FKL_DEF_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_DEF_STRUCT_TYPE_TAG:
			return t.st->align;
			break;
		case FKL_DEF_UNION_TYPE_TAG:
			return t.ut->align;
			break;
		case FKL_DEF_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

static void initStructTypeId(FklTypeId_t id,FklSid_t structNameId,uint32_t num,FklSid_t* symbols,FklTypeId_t* memberTypes)
{
	size_t totalSize=0;
	size_t prevAllign=1;
	for(uint32_t i=0
			;i<num
			;totalSize+=totalSize%prevAllign
			,totalSize+=fklFfiGetTypeSize(fklFfiGetTypeUnion(memberTypes[i]))
			,i++)
		prevAllign=fklFfiGetTypeAlign(fklFfiGetTypeUnion(memberTypes[i]));
	FklDefTypeUnion* pst=&GlobTypeUnionList.ul[id-1];
	FklDefStructType* ost=(FklDefStructType*)FKL_GET_TYPES_PTR(pst->st);
	ost=(FklDefStructType*)realloc(ost,sizeof(FklDefStructType)+num*sizeof(FklDefStructMember));
	FKL_ASSERT(ost,__func__);
	ost->type=structNameId;
	ost->num=num;
	ost->totalSize=totalSize;
	for(uint32_t i=0;i<num;i++)
	{
		ost->layout[i].memberSymbol=symbols[i];
		ost->layout[i].type=memberTypes[i];
	}
	pst->st=FKL_MAKE_STRUCT_TYPE(ost);
}

static FklTypeId_t genStructTypeId(FklVMvalue* structBodyPair,FklDefTypes* otherTypes)
{
	if(!FKL_IS_PAIR(structBodyPair))
		return 0;
	FklSid_t structNameId=0;
	FklVMvalue* structNameV=structBodyPair->u.pair->car;
	uint32_t num=0;
	FklVMvalue* memberV=NULL;
	FklVMvalue* memberPair=structBodyPair;
	if(!FKL_IS_PAIR(structNameV))
	{
		if(FKL_IS_SYM(structNameV))
			structNameId=FKL_GET_SYM(structNameV);
		else
			return 0;
		memberPair=structBodyPair->u.pair->cdr;
		memberV=FKL_IS_PAIR(memberPair)?memberPair->u.pair->car:NULL;
	}
	else
		memberV=structNameV;
	FklTypeId_t retval=0;
	if(memberV==NULL&&structNameId!=0)
	{
		FklTypeId_t i=0;
		FklTypeId_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_DEF_STRUCT_TYPE_TAG)
			{
				FklDefStructType* st=(FklDefStructType*)FKL_GET_TYPES_PTR(typeUnion.st);
				if(st->type==structNameId)
				{
					retval=i+1;
					break;
				}
			}
		}
		if(!retval)
			return retval;
	}
	else if(structNameId!=0)
		retval=fklFfiNewStructType(structNameId,0,NULL,NULL);
	for(FklVMvalue* first=memberPair;FKL_IS_PAIR(first);first=first->u.pair->cdr)
		num++;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberV,__func__);
		for(uint32_t i=0;FKL_IS_PAIR(memberPair);i++)
		{
			memberV=memberPair->u.pair->car;
			if(!FKL_IS_PAIR(memberV))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklVMvalue* memberName=memberV->u.pair->car;
			if(!FKL_IS_SYM(memberName))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklSid_t symbol=FKL_GET_SYM(memberName);
			FklVMvalue* memberTypeV=FKL_IS_PAIR(memberV->u.pair->cdr)?memberV->u.pair->cdr->u.pair->car:NULL;
			if(!memberTypeV||memberV->u.pair->cdr->u.pair->cdr!=FKL_VM_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklTypeId_t type=fklFfiGenDefTypesUnion(memberTypeV,otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			memberSymbolList[i]=symbol;
			memberTypeList[i]=type;
			memberPair=memberPair->u.pair->cdr;
			if(!FKL_IS_PAIR(memberPair)&&memberPair!=FKL_VM_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
		}
	}
	if(retval&&num)
		initStructTypeId(retval,structNameId,num,memberSymbolList,memberTypeList);
	else if(!retval)
		retval=fklFfiNewStructType(structNameId,num,memberSymbolList,memberTypeList);
	free(memberSymbolList);
	free(memberTypeList);
	return retval;
}

static void initUnionTypeId(FklTypeId_t id,FklSid_t unionNameId,uint32_t num,FklSid_t* symbols,FklTypeId_t* memberTypes)
{
	size_t maxSize=0;
	for(uint32_t i=0;i<num;i++)
	{
		size_t curSize=fklFfiGetTypeSizeWithTypeId(memberTypes[i]);
		if(maxSize<curSize)
			maxSize=curSize;
	}
	FklDefTypeUnion* put=&GlobTypeUnionList.ul[id-1];
	FklDefUnionType* out=(FklDefUnionType*)FKL_GET_TYPES_PTR(put->ut);
	out=(FklDefUnionType*)realloc(out,sizeof(FklDefUnionType)+num*sizeof(FklDefStructMember));
	FKL_ASSERT(out,__func__);
	out->type=unionNameId;
	out->num=num;
	out->maxSize=maxSize;
	for(uint32_t i=0;i<num;i++)
	{
		out->layout[i].memberSymbol=symbols[i];
		out->layout[i].type=memberTypes[i];
	}
	put->ut=FKL_MAKE_UNION_TYPE(out);
}

static FklTypeId_t genUnionTypeId(FklVMvalue* unionBodyPair,FklDefTypes* otherTypes)
{
	if(!FKL_IS_PAIR(unionBodyPair))
		return 0;
	FklSid_t unionNameId=0;
	FklVMvalue* unionNameV=unionBodyPair->u.pair->car;
	uint32_t num=0;
	FklVMvalue* memberV=NULL;
	FklVMvalue* memberPair=unionBodyPair;
	if(!FKL_IS_PAIR(unionNameV))
	{
		if(FKL_IS_SYM(unionNameV))
			unionNameId=FKL_GET_SYM(unionNameV);
		else
			return 0;
		memberPair=unionBodyPair->u.pair->cdr;
		memberV=FKL_IS_PAIR(memberPair)?memberPair->u.pair->car:NULL;
	}
	else
		memberV=unionNameV;
	FklTypeId_t retval=0;
	if(memberV==NULL&&unionNameId!=0)
	{
		FklTypeId_t i=0;
		uint32_t num=GlobTypeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion typeUnion=GlobTypeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_DEF_UNION_TYPE_TAG)
			{
				FklDefUnionType* ut=(FklDefUnionType*)FKL_GET_TYPES_PTR(typeUnion.st);
				if(ut->type==unionNameId)
				{
					retval=i+1;
					break;
				}
			}
		}
		if(!retval)
			return retval;
	}
	else if(unionNameId!=0)
		retval=fklFfiNewUnionType(unionNameId,0,NULL,NULL);
	for(FklVMvalue* first=memberPair;FKL_IS_PAIR(first);first=first->u.pair->cdr)
		num++;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberV,__func__);
		for(uint32_t i=0;FKL_IS_PAIR(memberPair);i++)
		{
			memberV=memberPair->u.pair->car;
			if(!FKL_IS_PAIR(memberV))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklVMvalue* memberName=memberV->u.pair->car;
			if(!FKL_IS_SYM(memberName))
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklSid_t symbol=FKL_GET_SYM(memberName);
			FklVMvalue* memberTypeV=FKL_IS_PAIR(memberV->u.pair->cdr)?memberV->u.pair->cdr->u.pair->car:NULL;
			if(!memberTypeV||memberV->u.pair->cdr->u.pair->cdr!=FKL_VM_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			FklTypeId_t type=fklFfiGenDefTypesUnion(memberTypeV,otherTypes);
			if(!type)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
			memberSymbolList[i]=symbol;
			memberTypeList[i]=type;
			memberPair=memberPair->u.pair->cdr;
			if(!FKL_IS_PAIR(memberPair)&&memberPair!=FKL_VM_NIL)
			{
				free(memberTypeList);
				free(memberSymbolList);
				return 0;
			}
		}
	}
	if(retval&&num)
		initUnionTypeId(retval,unionNameId,num,memberTypeList,memberTypeList);
	else if(!retval)
		retval=fklFfiNewUnionType(unionNameId,num,memberSymbolList,memberTypeList);
	free(memberSymbolList);
	free(memberTypeList);
	return retval;
}

static FklTypeId_t genFuncTypeId(FklVMvalue* functionBodyV,FklDefTypes* otherTypes)
{
    if(!FKL_IS_PAIR(functionBodyV))
        return 0;
    FklTypeId_t rtype=0;
    FklVMvalue* argV=functionBodyV->u.pair->car;
    if(!FKL_IS_PAIR(argV)&&argV!=FKL_VM_NIL)
        return 0;
    FklVMvalue* rtypeV=FKL_IS_PAIR(functionBodyV->u.pair->cdr)?functionBodyV->u.pair->cdr->u.pair->car:NULL;
	if((functionBodyV->u.pair->cdr!=FKL_VM_NIL
				&&!FKL_IS_PAIR(functionBodyV->u.pair->cdr))
			||(FKL_IS_PAIR(functionBodyV->u.pair->cdr)
				&&functionBodyV->u.pair->cdr->u.pair->cdr!=FKL_VM_NIL))
		return 0;
    if(rtypeV)
	{
		FklTypeId_t tmp=fklFfiGenDefTypesUnion(rtypeV,otherTypes);
		if(!tmp)
			return 0;
		rtype=tmp;
	}
    uint32_t i=0;
	if(FKL_IS_PAIR(argV))
	{
		i++;
		for(FklVMvalue* first=argV->u.pair->car;FKL_IS_PAIR(first);first=first->u.pair->cdr,i++);
	}
    FklTypeId_t* atypes=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*i);
    FKL_ASSERT(atypes,__func__);
    FklVMvalue* firArgV=argV;
    for(i=0;FKL_IS_PAIR(firArgV);firArgV=firArgV->u.pair->cdr,i++)
    {
        FklVMvalue* argV=firArgV->u.pair->car;
        FklTypeId_t tmp=fklFfiGenDefTypesUnion(argV,otherTypes);
        if(!tmp)
        {
            free(atypes);
            return 0;
        }
		FklVMvalue* cdr=firArgV->u.pair->cdr;
        if(!tmp||(!FKL_IS_PAIR(cdr)&&cdr!=FKL_VM_NIL))
        {
            free(atypes);
            return 0;
        }
        atypes[i]=tmp;
    }
    FklTypeId_t retval=fklFfiNewFuncType(rtype,i,atypes);
    free(atypes);
    return retval;
}

/*------------------------*/

FklTypeId_t fklFfiGenDefTypesUnion(FklVMvalue* obj,FklDefTypes* otherTypes)
{
	if(!otherTypes)
		return 0;
	if(FKL_IS_SYM(obj))
	{
		FklDefTypesNode* n=fklFfiFindDefTypesNode(FKL_GET_SYM(obj),otherTypes);
		if(!n)
			return 0;
		else
			return n->type;
	}
	else if(FKL_IS_PAIR(obj))
	{
		FklVMvalue* compositeDataHead=obj->u.pair->car;;
		if(!FKL_IS_SYM(compositeDataHead))
			return 0;
		FklSid_t sid=FKL_GET_SYM(compositeDataHead);
		if(sid==ArrayTypedefSymbolId)
			return genArrayTypeId(obj->u.pair->cdr,otherTypes);
		else if(sid==PtrTypedefSymbolId)
			return genPtrTypeId(obj->u.pair->cdr,otherTypes);
		else if(sid==StructTypedefSymbolId)
			return genStructTypeId(obj->u.pair->cdr,otherTypes);
		else if(sid==UnionTypedefSymbolId)
			return genUnionTypeId(obj->u.pair->cdr,otherTypes);
		else if(sid==FunctionTypedefSymbolId)
			return genFuncTypeId(obj->u.pair->cdr,otherTypes);
		else
			return 0;
	}
	else
		return 0;
}

static const char* FfiErrorType[]=
{
	"invalid-type-declare",
	"invalid-type-name",
	"invalid-mem-mode",
	"invalid-selector",
	"invalid-assign",
};

const char* fklFfiGetErrorType(FklFfiErrorType type)
{
	return FfiErrorType[type];
}

char* fklFfiGenErrorMessage(FklFfiErrorType type)
{
	char* t=fklCopyStr("");
	switch(type)
	{
		case FKL_FFI_INVALID_TYPEDECLARE:
			t=fklStrCat(t,"Invalid type declare ");
			break;
		case FKL_FFI_INVALID_TYPENAME:
			t=fklStrCat(t,"Invalid type name ");
			break;
		case FKL_FFI_INVALID_MEM_MODE:
			t=fklStrCat(t,"Invalid mem mode ");
			break;
		case FKL_FFI_INVALID_SELECTOR:
			t=fklStrCat(t,"Invalid selector ");
			break;
		case FKL_FFI_INVALID_ASSIGN:
			t=fklStrCat(t,"Invalid assign ");
			break;
		default:
			FKL_ASSERT(0,__func__);
			break;
	}
	return t;
}

int isNativeType(FklSid_t typeName,FklDefTypes* otherTypes)
{
	FklDefTypesNode* typeNode=fklFfiFindDefTypesNode(typeName,otherTypes);
	return typeNode!=NULL&&FKL_GET_TYPES_TAG(fklFfiGetTypeUnion(typeNode->type).all)==FKL_DEF_NATIVE_TYPE_TAG;
}

FklTypeId_t fklFfiGenDefTypes(FklVMvalue* obj,FklDefTypes* otherTypes,FklSid_t typeName)
{
	if(isNativeType(typeName,otherTypes))
		return 0;
	return fklFfiGenDefTypesUnion(obj,otherTypes);
}

int fklFfiIsNativeTypeName(FklSid_t typeName)
{
	pthread_rwlock_rdlock(&GlobDefTypesLock);
	int r=isNativeType(typeName,GlobDefTypes);
	pthread_rwlock_unlock(&GlobDefTypesLock);
	return r;
}

FklTypeId_t fklFfiTypedef(FklVMvalue* obj,FklSid_t typeName)
{
	pthread_rwlock_wrlock(&GlobDefTypesLock);
	FklTypeId_t retval=fklFfiGenDefTypesUnion(obj,GlobDefTypes);
	if(retval)
		fklFfiAddDefTypes(GlobDefTypes,typeName,retval);
	pthread_rwlock_unlock(&GlobDefTypesLock);
	return retval;
}

FklDefTypesNode* fklFfiFindDefTypesNode(FklSid_t typeName,FklDefTypes* otherTypes)
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
}

FklTypeId_t fklFfiGenTypeId(FklVMvalue* obj)
{
	return fklFfiGenDefTypesUnion(obj,GlobDefTypes);
}

int fklFfiIsNativeTypeId(FklTypeId_t type)
{
	return type>0&&type<=LastNativeTypeId;
}

int fklFfiIsFILEpTypeId(FklTypeId_t type)
{
	return type==FILEpTypeId;
}

int fklFfiIsStringTypeId(FklTypeId_t type)
{
	return type==StringTypeId;
}

int fklFfiIsArrayTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklFfiGetTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_ARRAY_TYPE_TAG;
}

int fklFfiIsPtrTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklFfiGetTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_PTR_TYPE_TAG;
}

int fklFfiIsStructTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklFfiGetTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_STRUCT_TYPE_TAG;
}

int fklFfiIsUnionTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklFfiGetTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_UNION_TYPE_TAG;
}

int fklFfiIsFunctionTypeId(FklTypeId_t type)
{
	FklDefTypeUnion tu=fklFfiGetTypeUnion(type);
	return FKL_GET_TYPES_TAG(tu.all)==FKL_DEF_FUNC_TYPE_TAG;
}

int fklFfiIsVptrTypeId(FklTypeId_t type)
{
	return type==LastNativeTypeId;
}

FklTypeId_t fklFfiGetI32TypeId(void)
{
	return I32TypeId;
}

FklTypeId_t fklFfiGetI64TypeId(void)
{
	return I64TypeId;
}

FklTypeId_t fklFfiGetCharTypeId(void)
{
	return CharTypeId;
}

FklTypeId_t fklFfiGetStringTypeId(void)
{
	return StringTypeId;
}

FklTypeId_t fklFfiGetF64TypeId(void)
{
	return F64TypeId;
}

FklTypeId_t fklFfiGetFILEpTypeId(void)
{
	return FILEpTypeId;
}

FklTypeId_t fklFfiGetLastNativeTypeId(void)
{
	return LastNativeTypeId;
}
