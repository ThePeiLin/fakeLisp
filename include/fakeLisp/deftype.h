#ifndef FKL_DEFTYPE_H
#define FKL_DEFTYPE_H
#include"symbol.h"
#include<stddef.h>

typedef uint32_t FklTypeId_t;
typedef enum
{
	FKL_DEF_NATIVE_TYPE_TAG=0,
	FKL_DEF_ARRAY_TYPE_TAG,
	FKL_DEF_PTR_TYPE_TAG,
	FKL_DEF_STRUCT_TYPE_TAG,
	FKL_DEF_UNION_TYPE_TAG,
	FKL_DEF_FUNC_TYPE_TAG,
}FklDefTypeTag;

typedef struct FklDefTypesNode
{
	FklSid_t name;
	FklTypeId_t type;
}FklDefTypesNode;

typedef struct FklDefTypes
{
	size_t num;
	FklDefTypesNode** u;
}FklDefTypes;

typedef union FklTypeUnion
{
	void* all;
	struct FklDefNativeType* nt;
	struct FklDefArrayType* at;
	struct FklDefPtrType* pt;
	struct FklDefStructType* st;
	struct FklDefUnionType* ut;
	struct FklDefFuncType* ft;
}FklTypeUnion;

typedef struct FklDefStructMember FklDefStructMember;
typedef struct FklDefNativeType
{
	FklSid_t type;
	uint32_t align;
	uint32_t size;
}FklDefNativeType;

typedef struct FklDefArrayType
{
	size_t num;
	size_t totalSize;
	uint32_t align;
	FklTypeId_t etype;
}FklDefArrayType;

typedef struct FklDefStructType
{
	int64_t type;
	uint32_t num;
	size_t totalSize;
	uint32_t align;
	struct FklDefStructMember
	{
		FklSid_t memberSymbol;
		FklTypeId_t type;
	}layout[];
}FklDefStructType;

typedef struct FklDefUnionType
{
	int64_t type;
	uint32_t num;
	size_t maxSize;
	uint32_t align;
	struct FklDefStructMember layout[];
}FklDefUnionType;

typedef struct FklDefFuncType
{
	FklTypeId_t rtype;
	uint32_t anum;
	FklTypeId_t atypes[];
}FklDefFuncType;

FklDefTypes* fklNewDefTypes(void);
void fklFreeDefTypeTable(FklDefTypes* defs);
#endif
