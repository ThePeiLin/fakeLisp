#include"fklffitype.h"
#include<fakeLisp/utils.h>
#include<string.h>

static pthread_rwlock_t GlobDefTypesLock=PTHREAD_RWLOCK_INITIALIZER;
static FklDefTypes* GlobDefTypes=NULL;

static FklTypeId_t LastNativeTypeId=0;
static FklTypeId_t CharTypeId=0;
static FklTypeId_t StringTypeId=0;
static FklTypeId_t FILEpTypeId=0;

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
		FklTypeId_t t=fklFfiNewNativeType(typeName,size,size);
		if(!CharTypeId&&!strcmp("char",nativeTypeList[i].typeName))
			CharTypeId=t;
		fklFfiAddDefTypes(otherTypes,typeName,t);
	}
	FklSid_t otherTypeName=fklAddSymbolToGlob("string")->id;
	StringTypeId=fklFfiNewNativeType(otherTypeName,sizeof(char*),sizeof(char*));
	fklFfiAddDefTypes(otherTypes,otherTypeName,StringTypeId);
	otherTypeName=fklAddSymbolToGlob("FILE*")->id;
	FILEpTypeId=fklFfiNewNativeType(otherTypeName,sizeof(FILE*),sizeof(FILE*));
	fklFfiAddDefTypes(otherTypes,otherTypeName,FILEpTypeId);
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

void fklFfiFreeVMStructType(FklDefStructType* obj)
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
