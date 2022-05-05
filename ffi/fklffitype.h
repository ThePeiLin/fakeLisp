#ifndef FKL_FFI_TYPE_H
#define FKL_FFI_TYPE_H
#include<fakeLisp/vm.h>
#define FKL_MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_NATIVE_TYPE_TAG))
#define FKL_MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_ARRAY_TYPE_TAG))
#define FKL_MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_PTR_TYPE_TAG))
#define FKL_MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_STRUCT_TYPE_TAG))
#define FKL_MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_UNION_TYPE_TAG))
#define FKL_MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_FUNC_TYPE_TAG))
#define FKL_GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_TYPES_TAG(P) ((FklDefTypeTag)(((uintptr_t)(P))&FKL_TAG_MASK))

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FklTypeId_t;
typedef enum
{
	FKL_DEF_NATIVE_TYPE_TAG=0,
	FKL_DEF_ARRAY_TYPE_TAG,
	FKL_DEF_PTR_TYPE_TAG,
	FKL_DEF_STRUCT_TYPE_TAG,
	FKL_DEF_UNION_TYPE_TAG,
	FKL_DEF_FUNC_TYPE_TAG,
}FklDefTypeTag;

typedef enum FklFfiErrorType
{
	FKL_FFI_INVALID_TYPEDECLARE=0,
	FKL_FFI_INVALID_TYPENAME,
	FKL_FFI_INVALID_MEM_MODE,
	FKL_FFI_INVALID_SELECTOR,
	FKL_FFI_INVALID_ASSIGN,
}FklFfiErrorType;

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
	FklSid_t type;
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
	FklSid_t type;
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

void fklFfiDefType(FklVMvalue* typedeclare);

FklTypeId_t fklFfiGetLastNativeTypeId(void);
FklTypeId_t fklFfiGetStringTypeId(void);
FklTypeId_t fklFfiGetFILEpTypeId(void);
FklTypeId_t fklFfiGetI32TypeId(void);
FklTypeId_t fklFfiGetI64TypeId(void);
FklTypeId_t fklFfiGetCharTypeId(void);
FklTypeId_t fklFfiGetF64TypeId(void);

FklDefTypes* fklFfiNewDefTypes(void);
void fklFfiFreeDefTypeTable(FklDefTypes* defs);

FklTypeId_t fklFfiGenDefTypes(FklVMvalue*,FklDefTypes* otherTypes,FklSid_t typeName);
FklTypeId_t fklFfiGenDefTypesUnion(FklVMvalue*,FklDefTypes* otherTypes);
FklDefTypesNode* fklFfiFindDefTypesNode(FklSid_t typeId,FklDefTypes* otherTypes);
int fklFfiAddDefTypes(FklDefTypes*,FklSid_t typeName,FklTypeId_t);
void fklFfiInitNativeDefTypes(FklDefTypes* otherTypes);

void fklFfiWriteTypeList(FILE* fp);
void fklFfiLoadTypeList(FILE* fp);
void fklFfiFreeGlobTypeList(void);

int fklFfiIsNativeTypeId(FklTypeId_t);
int fklFfiIsFILEpTypeId(FklTypeId_t);
int fklFfiIsStringTypeId(FklTypeId_t);
int fklFfiIsArrayTypeId(FklTypeId_t);
int fklFfiIsPtrTypeId(FklTypeId_t);
int fklFfiIsStructTypeId(FklTypeId_t);
int fklFfiIsUnionTypeId(FklTypeId_t);
int fklFfiIsFunctionTypeId(FklTypeId_t);
int fklFfiIsVptrTypeId(FklTypeId_t);

FklTypeId_t fklFfiGetI32TypeId(void);
FklTypeId_t fklFfiGetI64TypeId(void);
FklTypeId_t fklFfiCharTypeId(void);
FklTypeId_t fklFfiGetF64TypeId(void);

FklTypeId_t fklFfiNewNativeType(FklSid_t,size_t,size_t);
void fklFfiFreeNativeType(FklDefNativeType*);

FklTypeId_t fklFfiNewArrayType(FklTypeId_t,size_t);
void fklFfiFreeArrayType(FklDefArrayType*);

FklTypeId_t fklFfiNewPtrType(FklTypeId_t);
void fklFfiFreePtrType(FklDefPtrType*);

FklTypeId_t fklFfiNewStructType(FklSid_t,uint32_t,FklSid_t[],FklTypeId_t []);
void fklFfiFreeStructType(FklDefStructType*);

FklTypeId_t fklFfiNewUnionType(FklSid_t,uint32_t num,FklSid_t symbols[],FklTypeId_t memberTypes[]);
void fklFfiFreeUnionType(FklDefUnionType*);

FklTypeId_t fklFfiNewFuncType(FklTypeId_t rtype,uint32_t anum,FklTypeId_t atypes[]);
FklTypeId_t fklFfiFindSameFuncType(FklTypeId_t,uint32_t anum,FklTypeId_t atypes[]);
void fklFfiFreeFuncType(FklDefFuncType*);

size_t fklFfiGetTypeSize(FklDefTypeUnion t);
size_t fklFfiGetTypeAlign(FklDefTypeUnion t);
size_t fklFfiGetTypeSizeWithTypeId(FklTypeId_t t);
FklDefTypeUnion fklFfiGetTypeUnion(FklTypeId_t);

void fklFfiInitGlobNativeTypes(void);
void fklFfiFreeGlobDefTypeTable(void);
void fklFfiInitTypedefSymbol(void);

int fklFfiIsNativeTypeName(FklSid_t id);
const char* fklFfiGetErrorType(FklFfiErrorType);
char* fklFfiGenErrorMessage(FklFfiErrorType);

FklTypeId_t fklFfiGenTypeId(FklVMvalue*);
FklTypeId_t fklFfiTypedef(FklVMvalue*,FklSid_t typeName);
#define FKL_FFI_RAISE_ERROR(WHO,ERRORTYPE,EXE) do{\
	char* errorMessage=fklFfiGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklNiNewVMvalue(FKL_ERR\
			,fklNewVMerror((WHO)\
				,fklFfiGetErrorType(ERRORTYPE)\
				,errorMessage)\
			,(EXE)->stack\
			,(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#ifdef __cplusplus
}
#endif
#endif
