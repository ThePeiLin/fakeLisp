#ifndef FKL_FFI_MEM_H
#define FKL_FFI_MEM_H
#include"fklffitype.h"
typedef struct FklFfiMem
{
	FklTypeId_t type;
	void* mem;
}FklFfiMem;

void fklFfiMemInit(void);
FklSid_t fklFfiGetFfiMemUdSid(void);
FklFfiMem* fklFfiNewMem(FklTypeId_t type,size_t);
FklFfiMem* fklFfiNewRef(FklTypeId_t type,void* ref);
FklVMudata* fklFfiNewMemUd(FklTypeId_t type,size_t size,FklVMvalue* atomic);
FklVMudata* fklFfiNewMemRefUd(FklFfiMem* m,FklVMvalue* selector,FklVMvalue* index);
int fklFfiIsMem(FklVMvalue*);
int fklFfiSetMem(FklFfiMem*,FklVMvalue*);
int fklFfiIsNull(FklFfiMem*);
FklVMudata* fklFfiCastVMvalueIntoMem(FklVMvalue*);
int fklFfiIsCastableVMvalueType(FklVMvalue* v);
#endif
