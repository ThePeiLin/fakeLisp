#ifndef FAKE_FFI_H
#define FAKE_FFI_H
#include"deftype.h"
#include"vm.h"
#include<ffi.h>
void fklApplyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue);

ffi_type* fklGetFfiType(FklTypeId_t typeId);
void fklPrepFFIcif(ffi_cif* cif,int argc,ffi_type** atypes,ffi_type* rtype);
void fklApplyFlproc(FklVMflproc* f,void* rvalue,void** avalue);
int fklCastValueToVptr(FklTypeId_t,FklVMvalue* v,void** p);

FklVMmem* fklNewVMmem(FklTypeId_t typeId,uint8_t* mem);
void fklPrintMemoryRef(FklTypeId_t,FklVMmem*,FILE*);
FklVMvalue* fklMemoryCast(FklTypeId_t,FklVMmem*,FklVMheap*);
int fklMemorySet(FklTypeId_t id,FklVMmem*,FklVMvalue* v);
#endif