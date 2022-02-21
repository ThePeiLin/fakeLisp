#ifndef FAKE_FFI_H
#define FAKE_FFI_H
#include"fakedef.h"
#include<ffi.h>
void fklApplyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue);

ffi_type* fklGetFfiType(TypeId_t typeId);

int fklCastValueToVptr(TypeId_t,VMvalue* v,void** p);
#endif
