#ifndef FKL_FFI_MEM_H
#define FKL_FFI_MEM_H
#include"fklffitype.h"

#ifdef __cplusplus
extern "C"{
#endif
typedef struct FklFfiMem
{
	FklTypeId_t type;
	void* mem;
}FklFfiMem;

void fklFfiMemInit(FklVMvalue*);
FklSid_t fklFfiGetFfiMemUdSid(void);
FklFfiMem* fklFfiNewMem(FklTypeId_t type,size_t);
FklFfiMem* fklFfiNewRef(FklTypeId_t type,void* ref);
FklVMudata* fklFfiNewMemUd(FklTypeId_t type,size_t size,FklVMvalue* atomic);
FklVMudata* fklFfiNewMemRefUdWithSI(FklFfiMem* m,FklVMvalue* selector,FklVMvalue* index);
FklVMudata* fklFfiNewMemRefUd(FklTypeId_t type,void*);
int fklFfiIsMem(FklVMvalue*);
int fklFfiSetMem(FklFfiMem*,FklVMvalue*);
int fklFfiSetMemForProc(FklVMudata*,FklVMvalue*);
int fklFfiIsNull(FklFfiMem*);
FklVMudata* fklFfiCastVMvalueIntoMem(FklVMvalue*);
int fklFfiIsCastableVMvalueType(FklVMvalue* v);
int fklFfiIsValuableMem(FklFfiMem* mem);
FklVMvalue* fklFfiNewVMvalue(FklFfiMem* mem,FklVMstack* stack,FklVMgc* gc);
FklVMvalue* fklFfiGetRel(void);
#ifdef __cplusplus
}
#endif
#endif
