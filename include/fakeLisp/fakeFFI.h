#ifndef FAKE_FFI_H
#define FAKE_FFI_H
#include"fakedef.h"
#include<ffi.h>
void fklApplyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue);

ffi_type* fklGetFfiType(TypeId_t typeId);
void fklPrepFFIcif(ffi_cif* cif,int argc,ffi_type** atypes,ffi_type* rtype);
void fklApplyFlproc(VMFlproc* f,void* rvalue,void** avalue);
int fklCastValueToVptr(TypeId_t,VMvalue* v,void** p);
#endif
