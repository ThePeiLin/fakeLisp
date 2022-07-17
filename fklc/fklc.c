#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include"fbc.h"
#include"flnt.h"
#include"fsym.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

#define PREDICATE(condtion,err_infor) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(!val)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(condtion)\
	fklNiReturn(FKL_VM_TRUE,&ap,stack);\
	else\
	fklNiReturn(FKL_VM_NIL,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void fklc_fbc_p(ARGL) PREDICATE(fklcIsFbc(val),"fklc.fbc?")

#undef PREDICATE

#define IS_LITERAL(V) ((V)==FKL_VM_NIL||fklIsVMnumber(V)||FKL_IS_CHR(V)||FKL_IS_STR(V)||FKL_IS_SYM(V))

#define COMPILE_INTEGER(V) (FKL_IS_I32(V)?\
		fklNewPushI32ByteCode(FKL_GET_I32(V)):\
		FKL_IS_I64(V)?\
		fklNewPushI64ByteCode((V)->u.i64):\
		fklNewPushBigIntByteCode((V)->u.bigInt))

#define COMPILE_NUMBER(V) (fklIsInt(V)?COMPILE_INTEGER(V):fklNewPushF64ByteCode((V)->u.f64))

#define COMPILE_LITERAL(V) ((V)==FKL_VM_NIL?\
		fklNewPushNilByteCode():\
		fklIsVMnumber(V)?\
		COMPILE_NUMBER(V):\
		FKL_IS_CHR(V)?\
		fklNewPushCharByteCode(FKL_GET_CHR(V)):\
		FKL_IS_STR(V)?\
		fklNewPushStrByteCode((V)->u.str):\
		fklNewPushSidByteCode(FKL_GET_SYM(V)))

#define CONST_COMPILE(ERR_INFO,ARG,P,BC) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* ARG=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_TOOMANYARG,runnable,exe);\
	if(!ARG)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_TOOFEWARG,runnable,exe);\
	if(!P(ARG))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_WRONGARG,runnable,exe);\
	fklNiReturn(fklNewVMvalueToStack(FKL_USERDATA,fklcNewFbcUd(BC),stack,exe->heap),&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void fklc_fbc_append(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* retval=FKL_VM_NIL;
	FklByteCode* bc=NULL;
	for(FklVMvalue* cur=fklNiGetArg(&ap,stack);cur;cur=fklNiGetArg(&ap,stack))
	{
		FKL_NI_CHECK_TYPE(cur,fklcIsFbc,"fklc.fbc-append",runnable,exe);
		fklcCodeAppend(&bc,cur->u.ud->data);
	}
	if(bc)
		retval=fklNewVMvalueToStack(FKL_USERDATA,fklcNewFbcUd(bc),stack,exe->heap);
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_make_push_nil(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("make-push-nil",FKL_TOOMANYARG,runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_USERDATA,fklcNewFbcUd(fklNewPushNilByteCode()),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_compile_i32(ARGL) CONST_COMPILE("fklc.compile-i32",i_32,FKL_IS_I32,fklNewPushI32ByteCode(FKL_GET_I32(i_32)))
void fklc_compile_i64(ARGL) CONST_COMPILE("fklc.compile-i64",i_64,FKL_IS_I64,fklNewPushI64ByteCode(i_64->u.i64))
void fklc_compile_char(ARGL) CONST_COMPILE("fklc.compile-char",chr,FKL_IS_CHR,fklNewPushCharByteCode(FKL_GET_CHR(chr)))
void fklc_compile_symbol(ARGL) CONST_COMPILE("fklc.compile-symbol",sym,FKL_IS_SYM,fklNewPushSidByteCode(fklcGetSymbolIdWithOuterSymbolId(FKL_GET_SYM(sym))))
void fklc_compile_big_int(ARGL) CONST_COMPILE("fklc.compile-big-int",big_int,FKL_IS_BIG_INT,fklNewPushBigIntByteCode(big_int->u.bigInt))
void fklc_compile_f64(ARGL) CONST_COMPILE("fklc.compile-f64",f_64,FKL_IS_F64,fklNewPushF64ByteCode(f_64->u.f64))
void fklc_compile_string(ARGL) CONST_COMPILE("fklc.compile-string",str,FKL_IS_STR,fklNewPushStrByteCode(str->u.str))
void fklc_compile_integer(ARGL) CONST_COMPILE("fklc.compile-integer",integer,fklIsInt,COMPILE_INTEGER(integer))
void fklc_compile_number(ARGL) CONST_COMPILE("fklc.compile-number",number,fklIsVMnumber,COMPILE_NUMBER(number))
void fklc_compile_pair(ARGL) CONST_COMPILE("fklc.compile-pair",pair,FKL_IS_PAIR,fklcNewPushPairByteCode(pair))
void fklc_compile_vector(ARGL) CONST_COMPILE("fklc.compile-vector",vec,FKL_IS_VECTOR,fklcNewPushVectorByteCode(vec))
void fklc_compile_box(ARGL) CONST_COMPILE("fklc.compile-box",box,FKL_IS_BOX,fklcNewPushBoxByteCode(box))
void fklc_compile_atom_literal(ARGL) CONST_COMPILE("fklc.compile-atom-literal",literal,IS_LITERAL,COMPILE_LITERAL(literal))

#undef CONST_COMPILE
#undef IS_LITERAL
void _fklInit(FklSymbolTable* glob,FklVMvalue* rel)
{
	fklcInitFsym(glob);
	fklcInit(rel);
}

void _fklUninit(void)
{
	fklcUninitFsym();
}
