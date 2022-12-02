#ifndef FKL_FFI_MEM_H
#define FKL_FFI_MEM_H
#include"fklffitype.h"

#ifdef __cplusplus
extern "C"{
#endif
typedef struct FklFfiMem
{
	FklVMvalue* pd;
	FklTypeId_t type;
	void* mem;
}FklFfiMem;

void fklFfiMemInit(FklFfiPublicData* pd,FklSymbolTable* table);
FklFfiMem* fklFfiCreateMem(FklTypeId_t type,size_t,FklVMvalue* pd);
FklFfiMem* fklFfiCreateRef(FklTypeId_t type,void* ref,FklVMvalue* pd);
FklVMudata* fklFfiCreateMemUd(FklTypeId_t type,size_t size,FklVMvalue* atomic,FklVMvalue* rel,FklVMvalue* pd);
FklVMudata* fklFfiCreateMemRefUdWithSI(FklFfiMem* m,FklVMvalue* selector,FklVMvalue* index,FklVMvalue* rel,FklVMvalue* pd);
FklVMudata* fklFfiCreateMemRefUd(FklTypeId_t type,void*,FklVMvalue* rel,FklVMvalue* pd);
int fklFfiIsMem(FklVMvalue*);
int fklFfiSetMem(FklFfiMem*,FklVMvalue*,FklSymbolTable* table);
int fklFfiSetMemForProc(FklVMudata*,FklVMvalue*,FklSymbolTable* table);
int fklFfiIsNull(FklFfiMem*);
FklVMudata* fklFfiCastVMvalueIntoMem(FklVMvalue*,FklVMvalue* rel,FklVMvalue* pd,FklSymbolTable* table);
int fklFfiIsCastableVMvalueType(FklVMvalue* v);
int fklFfiIsValuableMem(FklFfiMem* mem);
FklVMvalue* fklFfiCreateVMvalue(FklFfiMem* mem,FklVM* vm);
#ifdef __cplusplus
}
#endif
#endif
