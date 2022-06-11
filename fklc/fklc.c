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

#undef PREDICATE

#define CONST_COMPILE(ERR_INFO,ARG,P,BC) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* ARG=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
		FKL_RAISE_BUILTIN_ERROR(ERR_INFO,FKL_TOOMANYARG,runnable,exe);\
	if(!ARG)\
		FKL_RAISE_BUILTIN_ERROR(ERR_INFO,FKL_TOOFEWARG,runnable,exe);\
	if(!P(ARG))\
		FKL_RAISE_BUILTIN_ERROR(ERR_INFO,FKL_WRONGARG,runnable,exe);\
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA,fklcNewFbcUd(BC),stack,exe->heap),&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void FKL_fklc_compile_i32(ARGL) CONST_COMPILE("fklc.compile-i32",i_32,FKL_IS_I32,fklNewPushI32ByteCode(FKL_GET_I32(i_32)))
void FKL_fklc_compile_i64(ARGL) CONST_COMPILE("fklc.compile-i64",i_64,FKL_IS_I64,fklNewPushI64ByteCode(i_64->u.i64))
void FKL_fklc_compile_char(ARGL) CONST_COMPILE("fklc.compile-char",chr,FKL_IS_CHR,fklNewPushCharByteCode(FKL_GET_CHR(chr)))
void FKL_fklc_compile_symbol(ARGL) CONST_COMPILE("fklc.compile-symbol",sym,FKL_IS_SYM,fklNewPushSidByteCode(fklcGetSymbolIdWithOuterSymbolId(FKL_GET_SYM(sym))))
void FKL_fklc_compile_big_int(ARGL) CONST_COMPILE("fklc.compile-big-int",big_int,FKL_IS_BIG_INT,fklNewPushBigIntByteCode(big_int->u.bigInt))
void FKL_fklc_compile_f64(ARGL) CONST_COMPILE("fklc.compile-f64",f_64,FKL_IS_F64,fklNewPushF64ByteCode(f_64->u.f64))
void FKL_fklc_compile_string(ARGL) CONST_COMPILE("fklc.compile-string",str,FKL_IS_STR,fklcNewPushStrByteCode(str->u.str))
#undef CONST_COMPILE

void _fklInit(FklSymbolTable* glob,FklVMvalue* rel)
{
	fklcInitFsym(glob);
	fklcInit(rel);
}

void _fklUninit(void)
{
	fklcUninitFsym();
}
