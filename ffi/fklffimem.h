#ifndef FKL_FFI_MEM_H
#define FKL_FFI_MEM_H
#include"fklffitype.h"
typedef struct FklFfiMem
{
	FklTypeId_t type;
	void* mem;
}FklFfiMem;

void fklFfiMemInit(void);
FklFfiMem* fklFfiNewMem(FklTypeId_t type,void* mem);
FklVMudata* fklFfiNewMemUd(FklTypeId_t type,void* mem);
int fklFfiIsMem(FklVMvalue*);
#endif
