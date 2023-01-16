#include"fklffitype.h"
#include<fakeLisp/utils.h>
#include<string.h>
#include<stdalign.h>
#ifdef _WIN32
#include<BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

void fklFfiInitGlobNativeTypes(FklFfiPublicData* pd,FklSymbolTable* table)
{
	pd->defTypes=fklFfiCreateDefTypes();
	pd->typeUnionList.num=0;
	pd->typeUnionList.ul=NULL;
	fklFfiInitNativeDefTypes(pd->defTypes,pd,table);
}

void fklFfiDestroyGlobDefTypeTable(FklFfiPublicData* pd)
{
	fklFfiDestroyDefTypeTable(pd->defTypes);
}

void fklFfiInitNativeDefTypes(FklDefTypes* otherTypes
		,FklFfiPublicData* pd
		,FklSymbolTable* table)
{
	struct
	{
		char* typeName;
		size_t size;
		size_t align;
	}nativeTypeList[]=
	{
		{"short",              sizeof(short),              alignof(short),              },
		{"int",                sizeof(int),                alignof(int),                },
		{"unsigned-short",     sizeof(unsigned short),     alignof(unsigned short),     },
		{"unsigned",           sizeof(unsigned),           alignof(unsigned),           },
		{"long",               sizeof(long),               alignof(long),               },
		{"unsigned-long",      sizeof(unsigned long),      alignof(unsigned long),      },
		{"long-long",          sizeof(long long),          alignof(long long),          },
		{"unsigned-long-long", sizeof(unsigned long long), alignof(unsigned long long), },
		{"ptrdiff_t",          sizeof(ptrdiff_t),          alignof(ptrdiff_t),          },
		{"size_t",             sizeof(size_t),             alignof(size_t),             },
		{"ssize_t",            sizeof(ssize_t),            alignof(ssize_t),            },
		{"char",               sizeof(char),               alignof(char),               },
		{"wchar_t",            sizeof(wchar_t),            alignof(wchar_t),            },
		{"float",              sizeof(float),              alignof(float),              },
		{"double",             sizeof(double),             alignof(double),             },
		{"int8_t",             sizeof(int8_t),             alignof(int8_t),             },
		{"uint8_t",            sizeof(uint8_t),            alignof(uint8_t),            },
		{"int16_t",            sizeof(int16_t),            alignof(int16_t),            },
		{"uint16_t",           sizeof(uint16_t),           alignof(uint16_t),           },
		{"int32_t",            sizeof(int32_t),            alignof(int32_t),            },
		{"uint32_t",           sizeof(uint32_t),           alignof(uint32_t),           },
		{"int64_t",            sizeof(int64_t),            alignof(int64_t),            },
		{"uint64_t",           sizeof(uint64_t),           alignof(uint64_t),           },
		{"iptr",               sizeof(intptr_t),           alignof(intptr_t),           },
		{"uptr",               sizeof(uintptr_t),          alignof(uintptr_t),          },
		{"vptr",               sizeof(void*),              alignof(void*),              },
		{"string",             sizeof(char*),              alignof(char*),              },
		{"FILE*",              sizeof(FILE*),              alignof(FILE*),              },
	};
	size_t num=sizeof(nativeTypeList)/sizeof(nativeTypeList[0]);
	size_t i=0;
	for(;i<num;i++)
	{
		FklSid_t typeName=fklAddSymbolCstr(nativeTypeList[i].typeName,table)->id;
		FklTypeId_t t=fklFfiCreateNativeType(typeName,nativeTypeList[i].size,nativeTypeList[i].align,pd);
		fklFfiAddDefTypes(otherTypes,typeName,t);
	}
}

static FklTypeId_t addToGlobTypeUnionList(FklDefTypeUnion type,FklFfiPublicData* pd)
{
	pd->typeUnionList.num+=1;
	size_t num=pd->typeUnionList.num;
	pd->typeUnionList.ul=(FklDefTypeUnion*)realloc(pd->typeUnionList.ul,sizeof(FklDefTypeUnion)*num);
	FKL_ASSERT(pd->typeUnionList.ul);
	pd->typeUnionList.ul[num-1]=type;
	return num;
}

size_t _member_hashFunc(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

void _member_destroyItem(void* item)
{
	free(item);
}

int _member_keyEqual(void* pkey0,void* pkey1)
{
	FklSid_t k0=*(FklSid_t*)pkey0;
	FklSid_t k1=*(FklSid_t*)pkey1;
	return k0==k1;
}

void* _member_getKey(void* item)
{
	return &((FklFfiMemberHashItem*)item)->key;
}

static FklHashTableMethodTable FfiMemberHashMethodTable=
{
	.__hashFunc=_member_hashFunc,
	.__destroyItem=_member_destroyItem,
	.__keyEqual=_member_keyEqual,
	.__getKey=_member_getKey,
};

static FklFfiMemberHashItem* createMemberHashItem(FklSid_t id,FklTypeId_t type,size_t offset)
{
	FklFfiMemberHashItem* r=(FklFfiMemberHashItem*)malloc(sizeof(FklFfiMemberHashItem));
	FKL_ASSERT(r);
	r->key=id;
	r->type=type;
	r->offset=offset;
	return r;
}


FklTypeId_t fklFfiCreateStructType(FklSid_t structName
		,uint32_t num
		,FklSid_t symbols[]
		,FklTypeId_t memberTypes[]
		,FklFfiPublicData* pd)
{
	FklTypeId_t id=0;
	if(structName)
	{
		size_t i=0;
		size_t num=pd->typeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion tmpType=pd->typeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_FFI_DEF_STRUCT_TYPE_TAG)
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
			FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(memberTypes[i],pd);
			size_t align=fklFfiGetTypeAlign(tu);
			size_t size=fklFfiGetTypeSize(tu);
			totalSize+=(totalSize%align)?align-totalSize%align:0;
			totalSize+=size;
			if(maxSize<size)
				maxSize=size;
			if(align>structAlign)
				structAlign=align;
		}
		totalSize+=(maxSize&&totalSize%maxSize)?maxSize-totalSize%maxSize:0;
		FklDefStructType* tmp=(FklDefStructType*)malloc(sizeof(FklDefStructType));
		FKL_ASSERT(tmp);
		tmp->type=structName;
		tmp->totalSize=totalSize;
		tmp->align=structAlign;
		tmp->layout=fklCreateHashTable(8,4,2,0.75,1,&FfiMemberHashMethodTable);
		size_t offset=0;
		for(uint32_t i=0;i<num;i++)
		{
			FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(memberTypes[i],pd);
			size_t align=fklFfiGetTypeAlign(tu);
			offset+=(offset%align)?align-offset%align:0;
			fklPutNoRpHashItem(createMemberHashItem(symbols[i],memberTypes[i],offset),tmp->layout);
			size_t memberSize=fklFfiGetTypeSize(tu);
			offset+=memberSize;
		}
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_STRUCT_TYPE(tmp),pd);
	}
	return id;
}

FklTypeId_t fklFfiCreatePtrType(FklTypeId_t type,FklFfiPublicData* pd)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=pd->typeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=pd->typeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_FFI_DEF_PTR_TYPE_TAG)
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
		FKL_ASSERT(tmp);
		tmp->ptype=type;
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_PTR_TYPE(tmp),pd);
	}
	return id;
}

FklTypeId_t fklFfiCreateFuncType(FklTypeId_t rtype
		,uint32_t anum
		,FklTypeId_t atypes[]
		,FklFfiPublicData* pd)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=pd->typeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=pd->typeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_FFI_DEF_FUNC_TYPE_TAG)
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
		FKL_ASSERT(tmp);
		tmp->rtype=rtype;
		tmp->anum=anum;
		uint32_t i=0;
		for(;i<anum;i++)
			tmp->atypes[i]=atypes[i];
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_FUNC_TYPE(tmp),pd);
	}
	return id;
}

FklTypeId_t fklFfiCreateArrayType(FklTypeId_t type,size_t num,FklFfiPublicData* pd)
{
	FklTypeId_t id=0;
	size_t i=0;
	size_t typeNum=pd->typeUnionList.num;
	for(;i<typeNum;i++)
	{
		FklDefTypeUnion tmpType=pd->typeUnionList.ul[i];
		if(FKL_GET_TYPES_TAG(tmpType.all)==FKL_FFI_DEF_ARRAY_TYPE_TAG)
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
		FKL_ASSERT(tmp);
		tmp->etype=type;
		tmp->num=num;
		FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(type,pd);
		tmp->totalSize=num*fklFfiGetTypeSize(tu);
		tmp->align=fklFfiGetTypeAlign(tu);
		return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_ARRAY_TYPE(tmp),pd);
	}
	else
		return id;
}

FklTypeId_t fklFfiCreateUnionType(FklSid_t unionName
		,uint32_t num
		,FklSid_t symbols[]
		,FklTypeId_t memberTypes[]
		,FklFfiPublicData* pd)
{
	size_t maxSize=0;
	size_t align=0;
	for(uint32_t i=0;i<num;i++)
	{
		FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(memberTypes[i],pd);
		size_t curSize=fklFfiGetTypeSize(tu);
		if(maxSize<curSize)
			maxSize=curSize;
		size_t ta=fklFfiGetTypeAlign(tu);
		if(ta>align)
			align=ta;
	}
	FklDefUnionType* tmp=(FklDefUnionType*)malloc(sizeof(FklDefUnionType));
	FKL_ASSERT(tmp);
	tmp->type=unionName;
	tmp->maxSize=maxSize;
	tmp->align=align;
	tmp->layout=fklCreateHashTable(8,4,2,0.75,1,&FfiMemberHashMethodTable);
	for(uint32_t i=0;i<num;i++)
		fklPutNoRpHashItem(createMemberHashItem(symbols[i],memberTypes[i],0),tmp->layout);
	return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_UNION_TYPE(tmp),pd);
}

FklDefTypes* fklFfiCreateDefTypes(void)
{
	FklDefTypes* tmp=(FklDefTypes*)malloc(sizeof(FklDefTypes));
	FKL_ASSERT(tmp);
	tmp->num=0;
	tmp->u=NULL;
	return tmp;
}

FklTypeId_t fklFfiCreateNativeType(FklSid_t type,size_t size,size_t align,FklFfiPublicData* pd)
{
	FklDefNativeType* tmp=(FklDefNativeType*)malloc(sizeof(FklDefNativeType));
	FKL_ASSERT(tmp);
	tmp->type=type;
	tmp->align=align;
	tmp->size=size;
	return addToGlobTypeUnionList((FklDefTypeUnion)FKL_MAKE_NATIVE_TYPE(tmp),pd);
}

void fklFfiDestroyNativeType(FklDefNativeType* obj)
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
		FKL_ASSERT(otherTypes->u&&node);
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
		FKL_ASSERT(otherTypes->u);
		FklDefTypesNode* node=(FklDefTypesNode*)malloc(sizeof(FklDefTypesNode));
		FKL_ASSERT(otherTypes->u&&node);
		node->name=typeName;
		node->type=type;
		for(;i>mid;i--)
			otherTypes->u[i]=otherTypes->u[i-1];
		otherTypes->u[mid]=node;
	}
	return 0;
}

void fklFfiDestroyDefTypeTable(FklDefTypes* defs)
{
	size_t i=0;
	size_t num=defs->num;
	for(;i<num;i++)
		free(defs->u[i]);
	free(defs->u);
	free(defs);
}

void fklFfiDestroyGlobTypeList(FklFfiPublicData* pd)
{
	FklTypeId_t num=pd->typeUnionList.num;
	FklDefTypeUnion* ul=pd->typeUnionList.ul;
	FklTypeId_t i=0;
	for(;i<num;i++)
	{
		FklDefTypeUnion tu=ul[i];
		FklDefTypeTag tag=FKL_GET_TYPES_TAG(tu.all);
		switch(tag)
		{
			case FKL_FFI_DEF_NATIVE_TYPE_TAG:
				fklFfiDestroyNativeType(tu.all);
				break;
			case FKL_FFI_DEF_PTR_TYPE_TAG:
				fklFfiDestroyPtrType(tu.all);
				break;
			case FKL_FFI_DEF_ARRAY_TYPE_TAG:
				fklFfiDestroyArrayType(tu.all);
				break;
			case FKL_FFI_DEF_STRUCT_TYPE_TAG:
				fklFfiDestroyStructType(tu.all);
				break;
			case FKL_FFI_DEF_UNION_TYPE_TAG:
				fklFfiDestroyUnionType(tu.all);
				break;
			case FKL_FFI_DEF_FUNC_TYPE_TAG:
				fklFfiDestroyFuncType(tu.all);
				break;
			default:
				break;
		}
	}
	free(ul);
}

void fklFfiDestroyPtrType(FklDefPtrType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiDestroyArrayType(FklDefArrayType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiDestroyStructType(FklDefStructType* obj)
{
	FklDefStructType* st=FKL_GET_TYPES_PTR(obj);
	fklDestroyHashTable(st->layout);
	free(st);
}

void fklFfiDestroyUnionType(FklDefUnionType* obj)
{
	FklDefUnionType* ut=FKL_GET_TYPES_PTR(obj);
	fklDestroyHashTable(ut->layout);
	free(ut);
}

void fklFfiDestroyFuncType(FklDefFuncType* obj)
{
	free(FKL_GET_TYPES_PTR(obj));
}

void fklFfiInitTypedefSymbol(FklFfiPublicData* pd,FklSymbolTable* table)
{
	pd->arrayTypedefSymbolId=fklAddSymbolCstr("array",table)->id;
	pd->ptrTypedefSymbolId=fklAddSymbolCstr("ptr",table)->id;
	pd->structTypedefSymbolId=fklAddSymbolCstr("struct",table)->id;
	pd->unionTypedefSymbolId=fklAddSymbolCstr("union",table)->id;
	pd->functionTypedefSymbolId=fklAddSymbolCstr("function",table)->id;
	pd->voidSymbolId=fklAddSymbolCstr("void",table)->id;
}

/*genTypeId functions list*/
static FklTypeId_t genArrayTypeId(FklVMvalue* numPair,FklDefTypes* otherTypes,FklFfiPublicData* pd)
{
	if(!fklIsList(numPair))
		return 0;
	FklVMvalue* numV=FKL_IS_PAIR(numPair)?numPair->u.pair->car:NULL;
	if(!numV||!fklIsInt(numV))
		return 0;
	FklVMvalue* typePair=numPair->u.pair->cdr;
	FklVMvalue* typeV=FKL_IS_PAIR(typePair)?typePair->u.pair->car:NULL;
	if(!typeV)
		return 0;
	FklTypeId_t type=fklFfiGenDefTypesUnion(typeV,otherTypes,pd);
	if(!type||type==FKL_FFI_TYPE_STRING)
		return type;
	return fklFfiCreateArrayType(type,fklGetInt(numV),pd);
}

static FklTypeId_t genPtrTypeId(FklVMvalue* typePair,FklDefTypes* otherTypes,FklFfiPublicData* pd)
{
	if(!fklIsList(typePair))
		return 0;
	FklVMvalue* ptrTypeV=FKL_IS_PAIR(typePair)?typePair->u.pair->car:NULL;
	if(!ptrTypeV)
		return 0;
	FklTypeId_t pType=fklFfiGenDefTypesUnion(ptrTypeV,otherTypes,pd);
	if(!pType)
		return pType;
	return fklFfiCreatePtrType(pType,pd);
}

FklDefTypeUnion fklFfiGetTypeUnion(FklTypeId_t t,FklFfiPublicData* pd)
{
	FklDefTypeUnion r=pd->typeUnionList.ul[t-1];
	return r;
}

FklDefTypeUnion fklFfiLockAndGetTypeUnion(FklTypeId_t t,FklFfiPublicData* pd)
{
	FklDefTypeUnion r=pd->typeUnionList.ul[t-1];
	return r;
}

size_t fklFfiGetTypeSize(FklDefTypeUnion t)
{
	FklDefTypeTag tag=FKL_GET_TYPES_TAG(t.all);
	t.all=FKL_GET_TYPES_PTR(t.all);
	switch(tag)
	{
		case FKL_FFI_DEF_NATIVE_TYPE_TAG:
			return t.nt->size;
			break;
		case FKL_FFI_DEF_ARRAY_TYPE_TAG:
			return t.at->totalSize;
			break;
		case FKL_FFI_DEF_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_FFI_DEF_STRUCT_TYPE_TAG:
			return t.st->totalSize;
			break;
		case FKL_FFI_DEF_UNION_TYPE_TAG:
			return t.ut->maxSize;
			break;
		case FKL_FFI_DEF_FUNC_TYPE_TAG:
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
		case FKL_FFI_DEF_NATIVE_TYPE_TAG:
			return t.nt->align;
			break;
		case FKL_FFI_DEF_ARRAY_TYPE_TAG:
			return t.at->align;
			break;
		case FKL_FFI_DEF_PTR_TYPE_TAG:
			return sizeof(void*);
			break;
		case FKL_FFI_DEF_STRUCT_TYPE_TAG:
			return t.st->align;
			break;
		case FKL_FFI_DEF_UNION_TYPE_TAG:
			return t.ut->align;
			break;
		case FKL_FFI_DEF_FUNC_TYPE_TAG:
			return sizeof(void*);
			break;
		default:
			return 0;
			break;
	}
}

static void initStructTypeId(FklTypeId_t id
		,FklSid_t structNameId
		,uint32_t num
		,FklSid_t* symbols
		,FklTypeId_t* memberTypes
		,FklFfiPublicData* pd)
{
	size_t totalSize=0;
	size_t prevAllign=1;
	size_t align=0;
	for(uint32_t i=0
			;i<num
			;totalSize+=totalSize%prevAllign,i++)
	{
		FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(memberTypes[i],pd);
		prevAllign=fklFfiGetTypeAlign(tu);
		if(prevAllign>align)
			align=prevAllign;
		totalSize+=fklFfiGetTypeSize(tu);
	}
	FklDefTypeUnion* pst=&pd->typeUnionList.ul[id-1];
	FklDefStructType* ost=(FklDefStructType*)FKL_GET_TYPES_PTR(pst->st);
	FklDefStructType* tost=(FklDefStructType*)realloc(ost,sizeof(FklDefStructType));
	FKL_ASSERT(tost);
	ost=tost;
	ost->type=structNameId;
	ost->totalSize=totalSize;
	ost->align=align;
	if(ost->layout)
		fklDestroyHashTable(ost->layout);
	ost->layout=fklCreateHashTable(8,4,2,0.75,1,&FfiMemberHashMethodTable);
	size_t offset=0;
	for(uint32_t i=0;i<num;i++)
	{
		FklDefTypeUnion tu=fklFfiGetTypeUnion(memberTypes[i],pd);
		size_t align=fklFfiGetTypeAlign(tu);
		offset+=(offset%align)?align-offset%align:0;
		fklPutNoRpHashItem(createMemberHashItem(symbols[i],memberTypes[i],offset),ost->layout);
		size_t memberSize=fklFfiGetTypeSize(tu);
		offset+=memberSize;
	}
	pst->st=FKL_MAKE_STRUCT_TYPE(ost);
}

static FklTypeId_t genStructTypeId(FklVMvalue* structBodyPair,FklDefTypes* otherTypes,FklFfiPublicData* pd)
{
	if(!fklIsList(structBodyPair))
		return 0;
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
		FklTypeId_t num=pd->typeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion typeUnion=pd->typeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_FFI_DEF_STRUCT_TYPE_TAG)
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
		retval=fklFfiCreateStructType(structNameId,0,NULL,NULL,pd);
	for(FklVMvalue* first=memberPair;FKL_IS_PAIR(first);first=first->u.pair->cdr)
		num++;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberV);
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
			FklTypeId_t type=fklFfiGenDefTypesUnion(memberTypeV,otherTypes,pd);
			if(!type||type==FKL_FFI_TYPE_STRING)
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
		initStructTypeId(retval,structNameId,num,memberSymbolList,memberTypeList,pd);
	else if(!retval)
		retval=fklFfiCreateStructType(structNameId,num,memberSymbolList,memberTypeList,pd);
	free(memberSymbolList);
	free(memberTypeList);
	return retval;
}

static void initUnionTypeId(FklTypeId_t id
		,FklSid_t unionNameId
		,uint32_t num
		,FklSid_t* symbols
		,FklTypeId_t* memberTypes
		,FklFfiPublicData* pd)
{
	size_t maxSize=0;
	size_t align=0;
	for(uint32_t i=0;i<num;i++)
	{
		FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(memberTypes[i],pd);
		size_t curSize=fklFfiGetTypeSize(tu);
		if(maxSize<curSize)
			maxSize=curSize;
		size_t ta=fklFfiGetTypeAlign(tu);
		if(ta>align)
			align=ta;
	}
	FklDefTypeUnion* put=&pd->typeUnionList.ul[id-1];
	FklDefUnionType* out=(FklDefUnionType*)FKL_GET_TYPES_PTR(put->ut);
	FklDefUnionType* tout=(FklDefUnionType*)realloc(out,sizeof(FklDefUnionType)+num);
	FKL_ASSERT(tout);
	out=tout;
	out->type=unionNameId;
	out->maxSize=maxSize;
	out->align=align;
	if(out->layout)
		fklDestroyHashTable(out->layout);
	out->layout=fklCreateHashTable(8,4,2,0.75,1,&FfiMemberHashMethodTable);
	for(uint32_t i=0;i<num;i++)
		fklPutNoRpHashItem(createMemberHashItem(symbols[i],memberTypes[i],0),out->layout);
	put->ut=FKL_MAKE_UNION_TYPE(out);
}

static FklTypeId_t genUnionTypeId(FklVMvalue* unionBodyPair
		,FklDefTypes* otherTypes
		,FklFfiPublicData* pd)
{
	if(!fklIsList(unionBodyPair))
		return 0;
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
		uint32_t num=pd->typeUnionList.num;
		for(;i<num;i++)
		{
			FklDefTypeUnion typeUnion=pd->typeUnionList.ul[i];
			if(FKL_GET_TYPES_TAG(typeUnion.all)==FKL_FFI_DEF_UNION_TYPE_TAG)
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
		retval=fklFfiCreateUnionType(unionNameId,0,NULL,NULL,pd);
	for(FklVMvalue* first=memberPair;FKL_IS_PAIR(first);first=first->u.pair->cdr)
		num++;
	FklSid_t* memberSymbolList=NULL;
	FklTypeId_t* memberTypeList=NULL;
	if(num)
	{
		memberSymbolList=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		memberTypeList=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*num);
		FKL_ASSERT(memberTypeList&&memberV);
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
			FklTypeId_t type=fklFfiGenDefTypesUnion(memberTypeV,otherTypes,pd);
			if(!type||type==FKL_FFI_TYPE_STRING)
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
		initUnionTypeId(retval,unionNameId,num,memberTypeList,memberTypeList,pd);
	else if(!retval)
		retval=fklFfiCreateUnionType(unionNameId,num,memberSymbolList,memberTypeList,pd);
	free(memberSymbolList);
	free(memberTypeList);
	return retval;
}

static FklTypeId_t genFuncTypeId(FklVMvalue* functionBodyV,FklDefTypes* otherTypes,FklFfiPublicData* pd)
{
	if(!fklIsList(functionBodyV))
		return 0;
    if(!FKL_IS_PAIR(functionBodyV))
        return 0;
    FklTypeId_t rtype=0;
    FklVMvalue* argV=functionBodyV->u.pair->car;
    if(!fklIsList(argV))
        return 0;
    FklVMvalue* rtypeV=FKL_IS_PAIR(functionBodyV->u.pair->cdr)?functionBodyV->u.pair->cdr->u.pair->car:NULL;
	if((functionBodyV->u.pair->cdr!=FKL_VM_NIL
				&&!rtypeV)
			||(rtypeV
				&&functionBodyV->u.pair->cdr->u.pair->cdr!=FKL_VM_NIL))
		return 0;
    if(rtypeV&&rtypeV!=FKL_VM_NIL)
	{
		if(FKL_IS_SYM(rtypeV)&&FKL_GET_SYM(rtypeV)==pd->voidSymbolId)
			rtype=0;
		else
		{
			FklTypeId_t tmp=fklFfiGenDefTypesUnion(rtypeV,otherTypes,pd);
			if(!tmp)
				return 0;
			rtype=tmp;
		}
	}
    uint32_t i=0;
	for(FklVMvalue* first=argV;FKL_IS_PAIR(first);first=first->u.pair->cdr)i++;
    FklTypeId_t* atypes=(FklTypeId_t*)malloc(sizeof(FklTypeId_t)*i);
    FKL_ASSERT(atypes);
    FklVMvalue* firArgV=argV;
    for(i=0;FKL_IS_PAIR(firArgV);firArgV=firArgV->u.pair->cdr,i++)
    {
        FklVMvalue* argV=firArgV->u.pair->car;
        FklTypeId_t tmp=fklFfiGenDefTypesUnion(argV,otherTypes,pd);
        if(!tmp)
        {
            free(atypes);
            return 0;
        }
		FklVMvalue* cdr=firArgV->u.pair->cdr;
        if(!tmp||(!fklIsList(cdr)))
        {
            free(atypes);
            return 0;
        }
        atypes[i]=tmp;
    }
    FklTypeId_t retval=fklFfiCreateFuncType(rtype,i,atypes,pd);
    free(atypes);
    return retval;
}

/*------------------------*/

FklTypeId_t fklFfiGenDefTypesUnion(FklVMvalue* obj,FklDefTypes* otherTypes,FklFfiPublicData* pd)
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
		if(sid==pd->arrayTypedefSymbolId)
			return genArrayTypeId(obj->u.pair->cdr,otherTypes,pd);
		else if(sid==pd->ptrTypedefSymbolId)
			return genPtrTypeId(obj->u.pair->cdr,otherTypes,pd);
		else if(sid==pd->structTypedefSymbolId)
			return genStructTypeId(obj->u.pair->cdr,otherTypes,pd);
		else if(sid==pd->unionTypedefSymbolId)
			return genUnionTypeId(obj->u.pair->cdr,otherTypes,pd);
		else if(sid==pd->functionTypedefSymbolId)
			return genFuncTypeId(obj->u.pair->cdr,otherTypes,pd);
		else
			return 0;
	}
	else
		return 0;
}

void fklInitErrorTypes(FklFfiPublicData* pd,FklSymbolTable* table)
{
	FklSid_t* const ffiErrorTypeId=pd->ffiErrorTypeId;
	static const char* ffiErrorType[]=
	{
		"invalid-type-declare",
		"invalid-type-name",
		"invalid-mem-mode",
		"invalid-selector",
		"invalid-assign",
	};
	for(size_t i=0;i<FKL_FFI_ERROR_NUM;i++)
		ffiErrorTypeId[i]=fklAddSymbolCstr(ffiErrorType[i],table)->id;
}

FklSid_t fklFfiGetErrorType(FklFfiErrorType type
		,const FklSid_t* ffiErrorTypeId)
{
	return ffiErrorTypeId[type];
}

#include<fakeLisp/base.h>

FklString* fklFfiGenErrorMessage(FklFfiErrorType type)
{
	FklString* t=NULL;
	switch(type)
	{
		case FKL_FFI_ERR_INVALID_TYPEDECLARE:
			t=fklCreateStringFromCstr("Invalid type declare ");
			break;
		case FKL_FFI_ERR_INVALID_TYPENAME:
			t=fklCreateStringFromCstr("Invalid type name ");
			break;
		case FKL_FFI_ERR_INVALID_MEM_MODE:
			t=fklCreateStringFromCstr("Invalid mem mode ");
			break;
		case FKL_FFI_ERR_INVALID_SELECTOR:
			t=fklCreateStringFromCstr("Invalid selector ");
			break;
		case FKL_FFI_ERR_INVALID_ASSIGN:
			t=fklCreateStringFromCstr("Invalid assign ");
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return t;
}

int isNativeType(FklSid_t typeName
		,FklDefTypes* otherTypes
		,FklFfiPublicData* pd)
{
	FklDefTypesNode* typeNode=fklFfiFindDefTypesNode(typeName,otherTypes);
	return typeNode!=NULL&&FKL_GET_TYPES_TAG(fklFfiLockAndGetTypeUnion(typeNode->type,pd).all)==FKL_FFI_DEF_NATIVE_TYPE_TAG;
}

FklTypeId_t fklFfiGenDefTypes(FklVMvalue* obj,FklDefTypes* otherTypes,FklSid_t typeName,FklFfiPublicData* pd)
{
	if(isNativeType(typeName,otherTypes,pd))
		return 0;
	return fklFfiGenDefTypesUnion(obj,otherTypes,pd);
}

int fklFfiIsNativeTypeName(FklSid_t typeName,FklFfiPublicData* pd)
{
	int r=isNativeType(typeName,pd->defTypes,pd);
	return r;
}

FklTypeId_t fklFfiTypedef(FklVMvalue* obj,FklSid_t typeName,FklFfiPublicData* pd)
{
	FklTypeId_t retval=fklFfiGenDefTypesUnion(obj,pd->defTypes,pd);
	if(retval)
		fklFfiAddDefTypes(pd->defTypes,typeName,retval);
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

FklTypeId_t fklFfiGenTypeId(FklVMvalue* obj,FklFfiPublicData* pd)
{
	return fklFfiGenDefTypesUnion(obj,pd->defTypes,pd);
}

int fklFfiIsNativeTypeId(FklTypeId_t type)
{
	return type>0&&type<=FKL_FFI_TYPE_VPTR;
}

int fklFfiIsIntegerTypeId(FklTypeId_t type)
{
	return (type>=FKL_FFI_TYPE_SHORT&&type<FKL_FFI_TYPE_FLOAT)
		||(type>=FKL_FFI_TYPE_INT8_T&&type<FKL_FFI_TYPE_VPTR);
}

int fklFfiIsNumTypeId(FklTypeId_t type)
{
	return type>0&&type<FKL_FFI_TYPE_VPTR;
}

int fklFfiIsFloatTypeId(FklTypeId_t type)
{
	return type==FKL_FFI_TYPE_FLOAT||type==FKL_FFI_TYPE_DOUBLE;
}

int fklFfiIsArrayType(FklDefTypeUnion tu)
{
	return FKL_GET_TYPES_TAG(tu.all)==FKL_FFI_DEF_ARRAY_TYPE_TAG;
}

int fklFfiIsPtrTypeId(FklTypeId_t type,FklDefTypeUnion tu)
{
	return type==FKL_FFI_TYPE_VPTR||FKL_GET_TYPES_TAG(tu.all)==FKL_FFI_DEF_PTR_TYPE_TAG;
}

int fklFfiIsStructType(FklDefTypeUnion tu)
{
	return FKL_GET_TYPES_TAG(tu.all)==FKL_FFI_DEF_STRUCT_TYPE_TAG;
}

int fklFfiIsUnionType(FklDefTypeUnion tu)
{
	return FKL_GET_TYPES_TAG(tu.all)==FKL_FFI_DEF_UNION_TYPE_TAG;
}

int fklFfiIsFunctionType(FklDefTypeUnion tu)
{
	return FKL_GET_TYPES_TAG(tu.all)==FKL_FFI_DEF_FUNC_TYPE_TAG;
}
