#ifndef FKL_DEFTYPE_H
#define FKL_DEFTYPE_H
#include"symbol.h"
#include"ast.h"
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
}FklDefTypeUnion;

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

typedef struct FklDefPtrType
{
	FklTypeId_t ptype;
}FklDefPtrType;

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

FklTypeId_t fklGetLastNativeTypeId(void);
FklTypeId_t fklGetCharTypeId(void);
FklTypeId_t fklGetStringTypeId(void);
FklTypeId_t fklGetFILEpTypeId(void);

FklDefTypes* fklNewDefTypes(void);
void fklFreeDefTypeTable(FklDefTypes* defs);

FklTypeId_t fklGenDefTypes(FklAstCptr*,FklDefTypes* otherTypes,FklSid_t* typeName);
FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklDefTypes* otherTypes);
FklDefTypesNode* fklFindDefTypesNode(FklSid_t typeId,FklDefTypes* otherTypes);
int fklAddDefTypes(FklDefTypes*,FklSid_t typeName,FklTypeId_t);
FklTypeId_t fklGenDefTypesUnion(FklAstCptr* objCptr,FklDefTypes* otherTypes);
void fklInitNativeDefTypes(FklDefTypes* otherTypes);

void fklWriteTypeList(FILE* fp);
void fklLoadTypeList(FILE* fp);
void fklFreeGlobTypeList(void);

int fklIsNativeTypeId(FklTypeId_t);
int fklIsArrayTypeId(FklTypeId_t);
int fklIsPtrTypeId(FklTypeId_t);
int fklIsStructTypeId(FklTypeId_t);
int fklIsUnionTypeId(FklTypeId_t);
int fklIsFunctionTypeId(FklTypeId_t);

FklTypeId_t fklNewVMNativeType(FklSid_t,size_t,size_t);
void fklFreeVMNativeType(FklDefNativeType*);

FklTypeId_t fklNewVMArrayType(FklTypeId_t,size_t);
void fklFreeVMArrayType(FklDefArrayType*);

FklTypeId_t fklNewVMPtrType(FklTypeId_t);
void fklFreeVMPtrType(FklDefPtrType*);

FklTypeId_t fklNewVMStructType(const char*,uint32_t,FklSid_t[],FklTypeId_t []);
void fklFreeVMStructType(FklDefStructType*);

FklTypeId_t fklNewVMUnionType(const char* structName,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[]);
void fklFreeVMUnionType(FklDefUnionType*);

FklTypeId_t fklNewVMFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[]);
FklTypeId_t fklFindSameFuncType(FklTypeId_t,uint32_t anum,FklTypeId_t atypes[]);
void fklFreeVMFuncType(FklDefFuncType*);

size_t fklGetVMTypeSize(FklDefTypeUnion t);
size_t fklGetVMTypeAlign(FklDefTypeUnion t);
size_t fklGetVMTypeSizeWithTypeId(FklTypeId_t t);
FklDefTypeUnion fklGetVMTypeUnion(FklTypeId_t);

#endif
