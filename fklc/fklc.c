#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/opcode.h>
#include"fbc.h"
#include"flnt.h"
#include"fsym.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

#define PREDICATE(condtion,err_infor) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(!val)\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(condtion)\
		fklNiReturn(FKL_VM_TRUE,&ap,stack);\
	else\
		fklNiReturn(FKL_VM_NIL,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void FKL_fklc_fbc_p(ARGL) PREDICATE(fklcIsFbc(val),"fklc.fbc?")

void FKL_fklc_compile_i32(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* i_32=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("fklc.compile-i32",FKL_TOOMANYARG,runnable,exe);
	if(!i_32)
		FKL_RAISE_BUILTIN_ERROR("fklc.compile-i32",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_I32(i_32))
		FKL_RAISE_BUILTIN_ERROR("fklc.compile-i32",FKL_WRONGARG,runnable,exe);
	FklByteCode* bc=fklNewByteCode(sizeof(char)+sizeof(uint32_t));
	bc->code[0]=FKL_PUSH_I32;
	fklSetI32ToByteCode(bc->code+sizeof(char),FKL_GET_I32(i_32));
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA,fklcNewFbcUd(bc),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void _fklInit(FklSymbolTable* glob,FklVMvalue* rel)
{
	fklcInitFsym(glob);
	fklcInit(rel);
}

void _fklUninit(void)
{
	fklcUninitFsym();
}
