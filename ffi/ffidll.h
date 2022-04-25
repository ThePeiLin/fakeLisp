#ifndef FKL_FFI_DLL_H
#define FKL_FFI_DLL_H
#include<fakeLisp/vm.h>
void fklAddSharedObj(FklVMdllHandle);
#define FKL_MAKE_NATIVE_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_NATIVE_TYPE_TAG))
#define FKL_MAKE_ARRAY_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_ARRAY_TYPE_TAG))
#define FKL_MAKE_PTR_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_PTR_TYPE_TAG))
#define FKL_MAKE_STRUCT_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_STRUCT_TYPE_TAG))
#define FKL_MAKE_UNION_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_UNION_TYPE_TAG))
#define FKL_MAKE_FUNC_TYPE(P) ((void*)(((uintptr_t)(P))|FKL_DEF_FUNC_TYPE_TAG))
#define FKL_GET_TYPES_PTR(P) ((void*)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_TYPES_TAG(P) ((FklDefTypeTag)(((uintptr_t)(P))&FKL_TAG_MASK))

#endif
