#include"fakeFFI.h"

static ffi_type* NativeFFITypeList[]=
{
	&ffi_type_void,
	&ffi_type_sshort,
	&ffi_type_sint,
	&ffi_type_ushort,
	&ffi_type_uint,
	&ffi_type_slong,
	&ffi_type_ulong,
	&ffi_type_slong,//long long
	&ffi_type_ulong,//unsigned long long
	&ffi_type_slong,//ptrdiff_t
	&ffi_type_ulong,//size_t
	&ffi_type_slong,//ssize_t
	&ffi_type_schar,
	&ffi_type_sint32,//wchar_t
	&ffi_type_float,
	&ffi_type_double,
	&ffi_type_sint8,
	&ffi_type_uint8,
	&ffi_type_sint16,
	&ffi_type_uint16,
	&ffi_type_sint32,
	&ffi_type_uint32,
	&ffi_type_sint64,
	&ffi_type_uint64,
	&ffi_type_slong,//intptr_t
	&ffi_type_ulong,//uintptr_t
	&ffi_type_pointer,//void*
};

void applyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue)
{
	ffi_cif cif;
	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, rtype, atypes);
	ffi_call(&cif, func, rvalue, avalue);
}
