#ifndef FAKE_FFI_H
#define FAKE_FFI_H
#include"fakedef.h"
#include<ffi.h>
void applyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue);

ffi_type* getFfiType(TypeId_t typeId);

int castValueToVptr(TypeId_t,VMvalue* v,void** p);
#endif
