#ifndef FAKE_FFI_H
#define FAKE_FFI_H
#include<ffi.h>
void applyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue);
#endif
