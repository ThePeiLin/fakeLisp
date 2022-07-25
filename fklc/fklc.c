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
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!val)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,runnable,exe);\
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
		FKL_IS_SYM(V)?\
		fklNewPushSidByteCode(FKL_GET_SYM(V)):\
		fklNewPushBvecByteCode((V)->u.bvec))

#define CONST_COMPILE(ERR_INFO,ARG,P,BC) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* ARG=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOMANYARG,runnable,exe);\
	if(!ARG)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOFEWARG,runnable,exe);\
	if(!P(ARG))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_WRONGARG,runnable,exe);\
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(BC),stack,exe->heap),&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void fklc_make_push_nil(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-push-nil",FKL_ERR_TOOMANYARG,runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(fklNewPushNilByteCode()),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_fbc_to_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fbc=fklNiGetArg(&ap,stack);
	if(!fbc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(fbc,fklcIsFbc,"fklc.fbc->bytevector",runnable,exe);
	FklByteCode* bc=fbc->u.ud->data;
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(bc->size,bc->code),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_bytevector_to_fbc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* bv=fklNiGetArg(&ap,stack);
	if(!bv)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,runnable,exe);
	FKL_NI_CHECK_TYPE(bv,FKL_IS_BYTEVECTOR,"fklc.fbc->bytevector",runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(fklNewByteCodeAndInit(bv->u.bvec->size,bv->u.bvec->ptr)),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_compile_i32(ARGL) CONST_COMPILE("fklc.compile-i32",i_32,FKL_IS_I32,fklNewPushI32ByteCode(FKL_GET_I32(i_32)))
void fklc_compile_i64(ARGL) CONST_COMPILE("fklc.compile-i64",i_64,FKL_IS_I64,fklNewPushI64ByteCode(i_64->u.i64))
void fklc_compile_char(ARGL) CONST_COMPILE("fklc.compile-char",chr,FKL_IS_CHR,fklNewPushCharByteCode(FKL_GET_CHR(chr)))
void fklc_compile_symbol(ARGL) CONST_COMPILE("fklc.compile-symbol",sym,FKL_IS_SYM,fklNewPushSidByteCode(fklcGetSymbolIdWithOuterSymbolId(FKL_GET_SYM(sym))))
void fklc_compile_big_int(ARGL) CONST_COMPILE("fklc.compile-big-int",big_int,FKL_IS_BIG_INT,fklNewPushBigIntByteCode(big_int->u.bigInt))
void fklc_compile_f64(ARGL) CONST_COMPILE("fklc.compile-f64",f_64,FKL_IS_F64,fklNewPushF64ByteCode(f_64->u.f64))
void fklc_compile_string(ARGL) CONST_COMPILE("fklc.compile-string",str,FKL_IS_STR,fklNewPushStrByteCode(str->u.str))
void fklc_compile_bytevector(ARGL) CONST_COMPILE("fklc.compile-bytevector",bvec,FKL_IS_BYTEVECTOR,fklNewPushBvecByteCode(bvec->u.bvec))
void fklc_compile_integer(ARGL) CONST_COMPILE("fklc.compile-integer",integer,fklIsInt,COMPILE_INTEGER(integer))
void fklc_compile_number(ARGL) CONST_COMPILE("fklc.compile-number",number,fklIsVMnumber,COMPILE_NUMBER(number))
void fklc_compile_pair(ARGL) CONST_COMPILE("fklc.compile-pair",pair,FKL_IS_PAIR,fklcNewPushPairByteCode(pair))
void fklc_compile_vector(ARGL) CONST_COMPILE("fklc.compile-vector",vec,FKL_IS_VECTOR,fklcNewPushVectorByteCode(vec))
void fklc_compile_box(ARGL) CONST_COMPILE("fklc.compile-box",box,FKL_IS_BOX,fklcNewPushBoxByteCode(box))
void fklc_compile_atom_literal(ARGL) CONST_COMPILE("fklc.compile-atom-literal",literal,IS_LITERAL,COMPILE_LITERAL(literal))

#undef CONST_COMPILE
#undef IS_LITERAL
void _fklInit(FklSymbolTable* glob,FklVMvalue* rel,FklVMlist* GlobVMs)
{
	fklSetGlobVMs(GlobVMs);
	fklcInitFsym(glob);
	fklcInit(rel);
}

void _fklUninit(void)
{
	fklcUninitFsym();
}
