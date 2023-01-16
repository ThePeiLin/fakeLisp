#ifndef FKL_FFI_TYPE_H
#define FKL_FFI_TYPE_H
#include<fakeLisp/vm.h>
#define FKL_MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_FFI_DEF_NATIVE_TYPE_TAG))
#define FKL_MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_FFI_DEF_ARRAY_TYPE_TAG))
#define FKL_MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_FFI_DEF_PTR_TYPE_TAG))
#define FKL_MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_FFI_DEF_STRUCT_TYPE_TAG))
#define FKL_MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_FFI_DEF_UNION_TYPE_TAG))
#define FKL_MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_FFI_DEF_FUNC_TYPE_TAG))
#define FKL_GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_TYPES_TAG(P) ((FklDefTypeTag)(((uintptr_t)(P))&FKL_TAG_MASK))

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FklTypeId_t;
typedef enum
{
	FKL_FFI_DEF_NATIVE_TYPE_TAG=0,
	FKL_FFI_DEF_ARRAY_TYPE_TAG,
	FKL_FFI_DEF_PTR_TYPE_TAG,
	FKL_FFI_DEF_STRUCT_TYPE_TAG,
	FKL_FFI_DEF_UNION_TYPE_TAG,
	FKL_FFI_DEF_FUNC_TYPE_TAG,
}FklDefTypeTag;

typedef enum FklFfiErrorType
{
	FKL_FFI_ERR_INVALID_TYPEDECLARE=0,
	FKL_FFI_ERR_INVALID_TYPENAME,
	FKL_FFI_ERR_INVALID_MEM_MODE,
	FKL_FFI_ERR_INVALID_SELECTOR,
	FKL_FFI_ERR_INVALID_ASSIGN,
}FklFfiErrorType;

#define FKL_FFI_ERROR_NUM (FKL_FFI_ERR_INVALID_ASSIGN+1)
typedef enum FklFfiNativeTypeEnum
{
	FKL_FFI_TYPE_SHORT=1,
	FKL_FFI_TYPE_INT,
	FKL_FFI_TYPE_USHORT,
	FKL_FFI_TYPE_UNSIGNED,
	FKL_FFI_TYPE_LONG,
	FKL_FFI_TYPE_ULONG,
	FKL_FFI_TYPE_LONG_LONG,
	FKL_FFI_TYPE_ULONG_LONG,
	FKL_FFI_TYPE_PTRDIFF_T,
	FKL_FFI_TYPE_SIZE_T,
	FKL_FFI_TYPE_SSIZE_T,
	FKL_FFI_TYPE_CHAR,
	FKL_FFI_TYPE_WCHAR_T,
	FKL_FFI_TYPE_FLOAT,
	FKL_FFI_TYPE_DOUBLE,
	FKL_FFI_TYPE_INT8_T,
	FKL_FFI_TYPE_UINT8_T,
	FKL_FFI_TYPE_INT16_T,
	FKL_FFI_TYPE_UINT16_T,
	FKL_FFI_TYPE_INT32_T,
	FKL_FFI_TYPE_UINT32_T,
	FKL_FFI_TYPE_INT64_T,
	FKL_FFI_TYPE_UINT64_T,
	FKL_FFI_TYPE_IPTR,
	FKL_FFI_TYPE_UPTR,
	FKL_FFI_TYPE_VPTR,
	FKL_FFI_TYPE_STRING,
	FKL_FFI_TYPE_FILE_P,
}FklFfiNativeTypeEnum;

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

#ifdef _WIN32
#include<windows.h>
typedef HMODULE FklFfidllHandle;
#else
#include<dlfcn.h>
typedef void* FklFfidllHandle;
#endif

typedef struct FklFfiSharedObjNode
{
	FklFfidllHandle dll;
	struct FklFfiSharedObjNode* next;
}FklFfiSharedObjNode;

typedef struct FklFfiPublicData
{
	FklSid_t memUdSid;
	FklSid_t atomicSid;
	FklSid_t rawSid;
	FklSid_t refSid;
	FklSid_t deRefSid;
	FklSid_t arrayTypedefSymbolId;
	FklSid_t ptrTypedefSymbolId;
	FklSid_t structTypedefSymbolId;
	FklSid_t unionTypedefSymbolId;
	FklSid_t functionTypedefSymbolId;
	FklSid_t voidSymbolId;
	FklDefTypes* defTypes;
	struct
	{
		FklTypeId_t num;
		FklDefTypeUnion* ul;
	}typeUnionList;
	FklFfiSharedObjNode *sharedObjs;
	FklSid_t ffiErrorTypeId[5];
}FklFfiPublicData;

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

typedef struct
{
	FklSid_t key;
	FklTypeId_t type;
	size_t offset;
}FklFfiMemberHashItem;

typedef struct FklDefStructType
{
	FklSid_t type;
	size_t totalSize;
	uint32_t align;
	FklHashTable* layout;
}FklDefStructType;

typedef struct FklDefUnionType
{
	FklSid_t type;
	size_t maxSize;
	uint32_t align;
	FklHashTable* layout;
}FklDefUnionType;

typedef struct FklDefFuncType
{
	FklTypeId_t rtype;
	uint32_t anum;
	FklTypeId_t atypes[];
}FklDefFuncType;

void fklFfiDefType(FklVMvalue* typedeclare);

FklDefTypes* fklFfiCreateDefTypes(void);
void fklFfiDestroyDefTypeTable(FklDefTypes* defs);

FklTypeId_t fklFfiGenDefTypes(FklVMvalue*,FklDefTypes* otherTypes,FklSid_t typeName,FklFfiPublicData* pd);
FklTypeId_t fklFfiGenDefTypesUnion(FklVMvalue*,FklDefTypes* otherTypes,FklFfiPublicData* pd);
FklDefTypesNode* fklFfiFindDefTypesNode(FklSid_t typeId,FklDefTypes* otherTypes);
int fklFfiAddDefTypes(FklDefTypes*,FklSid_t typeName,FklTypeId_t);
void fklFfiInitNativeDefTypes(FklDefTypes* otherTypes,FklFfiPublicData*,FklSymbolTable* table);

void fklFfiWriteTypeList(FILE* fp);
void fklFfiLoadTypeList(FILE* fp);
void fklFfiDestroyGlobTypeList(FklFfiPublicData* pd);

int fklFfiIsNativeTypeId(FklTypeId_t);
int fklFfiIsIntegerTypeId(FklTypeId_t);
int fklFfiIsNumTypeId(FklTypeId_t type);
int fklFfiIsFloatTypeId(FklTypeId_t);

int fklFfiIsArrayType(FklDefTypeUnion tu);

int fklFfiIsPtrTypeId(FklTypeId_t tid,FklDefTypeUnion tu);

int fklFfiIsStructType(FklDefTypeUnion tu);
int fklFfiIsUnionType(FklDefTypeUnion tu);
int fklFfiIsFunctionType(FklDefTypeUnion tu);

FklTypeId_t fklFfiCreateNativeType(FklSid_t,size_t,size_t,FklFfiPublicData* pd);
void fklFfiDestroyNativeType(FklDefNativeType*);

FklTypeId_t fklFfiCreateArrayType(FklTypeId_t,size_t,FklFfiPublicData* pd);
void fklFfiDestroyArrayType(FklDefArrayType*);

FklTypeId_t fklFfiCreatePtrType(FklTypeId_t,FklFfiPublicData* pd);
void fklFfiDestroyPtrType(FklDefPtrType*);

FklTypeId_t fklFfiCreateStructType(FklSid_t
		,uint32_t
		,FklSid_t[]
		,FklTypeId_t []
		,FklFfiPublicData* pd);
void fklFfiDestroyStructType(FklDefStructType*);

FklTypeId_t fklFfiCreateUnionType(FklSid_t
		,uint32_t num
		,FklSid_t symbols[]
		,FklTypeId_t memberTypes[]
		,FklFfiPublicData* pd);
void fklFfiDestroyUnionType(FklDefUnionType*);

FklTypeId_t fklFfiCreateFuncType(FklTypeId_t rtype
		,uint32_t anum
		,FklTypeId_t atypes[]
		,FklFfiPublicData* pd);
FklTypeId_t fklFfiFindSameFuncType(FklTypeId_t
		,uint32_t anum
		,FklTypeId_t atypes[]
		,FklFfiPublicData* pd);
void fklFfiDestroyFuncType(FklDefFuncType*);

size_t fklFfiGetTypeSize(FklDefTypeUnion t);
size_t fklFfiGetTypeAlign(FklDefTypeUnion t);
FklDefTypeUnion fklFfiLockAndGetTypeUnion(FklTypeId_t,FklFfiPublicData* pd);
FklDefTypeUnion fklFfiGetTypeUnion(FklTypeId_t,FklFfiPublicData* pd);

void fklFfiInitGlobNativeTypes(FklFfiPublicData*,FklSymbolTable* table);
void fklFfiDestroyGlobDefTypeTable(FklFfiPublicData*);
void fklFfiInitTypedefSymbol(FklFfiPublicData* pd,FklSymbolTable* table);

int fklFfiIsNativeTypeName(FklSid_t id,FklFfiPublicData* pd);
FklSid_t fklFfiGetErrorType(FklFfiErrorType,const FklSid_t*);
FklString* fklFfiGenErrorMessage(FklFfiErrorType);

void fklInitErrorTypes(FklFfiPublicData*,FklSymbolTable* table);
FklTypeId_t fklFfiGenTypeId(FklVMvalue*,FklFfiPublicData* pd);
FklTypeId_t fklFfiTypedef(FklVMvalue*,FklSid_t typeName,FklFfiPublicData* pd);

#define FKL_FFI_RAISE_ERROR(WHO,ERRORTYPE,EXE,PB) do{\
	FklString* errorMessage=fklFfiGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR\
			,fklCreateVMerrorCstr((WHO)\
				,fklFfiGetErrorType((ERRORTYPE),(PB)->ffiErrorTypeId)\
				,errorMessage)\
			,(EXE));\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#ifdef __cplusplus
}
#endif
#endif
