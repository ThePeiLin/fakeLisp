#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include"fbc.h"
#include"flnt.h"
#include"fsym.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

#define FKLC_RAISE_ERROR(WHO,ERRORTYPE,EXE) do{\
	char* errorMessage=fklcGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklNewVMvalueToStack(FKL_TYPE_ERR\
			,fklNewVMerrorCstr((WHO)\
				,fklcGetErrorType(ERRORTYPE)\
				,errorMessage)\
			,(EXE)->stack\
			,(EXE)->gc);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)


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

void fklc_pattern_match(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* pattern=fklNiGetArg(&ap,stack);
	FklVMvalue* exp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.pattern-match",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!fklcIsValidSyntaxPattern(pattern))
		FKLC_RAISE_ERROR("fklc.pattern-match",FKL_FKLC_ERR_INVALID_SYNTAX_PATTERN,exe);
	FklVMhashTable* hash=fklNewVMhashTable(FKL_VM_HASH_EQ);
	fklcMatchPattern(pattern,exp,hash,exe->gc);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_HASHTABLE,hash,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

#define IS_LITERAL(V) ((V)==FKL_VM_NIL||fklIsVMnumber(V)||FKL_IS_CHR(V)||FKL_IS_STR(V)||FKL_IS_BYTEVECTOR(V)||FKL_IS_SYM(V))

#define IS_COMPILABLE(V) (IS_LITERAL(V)||FKL_IS_PAIR(V)||FKL_IS_BOX(V)||FKL_IS_VECTOR(V))

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
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(BC),stack,exe->gc),&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void fklc_make_push_nil(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-push-nil",FKL_ERR_TOOMANYARG,runnable,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(fklNewPushNilByteCode()),stack,exe->gc),&ap,stack);
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
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(bc->size,bc->code),stack,exe->gc),&ap,stack);
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
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(fklNewByteCodeAndInit(bv->u.bvec->size,bv->u.bvec->ptr)),stack,exe->gc),&ap,stack);
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
void fklc_compile_atom_literal(ARGL) CONST_COMPILE("fklc.compile-atom-literal",literal,IS_LITERAL,COMPILE_LITERAL(literal))
void fklc_compile_obj(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.compile-obj",FKL_ERR_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.compile-obj",FKL_ERR_TOOFEWARG,runnable,exe);
	if(!IS_COMPILABLE(obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.compile-obj",FKL_ERR_WRONGARG,runnable,exe);
	FklByteCode* bc=fklcNewPushObjByteCode(obj);
	if(!bc)
		FKLC_RAISE_ERROR("fklc.compile-obj",FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklcNewFbcUd(bc),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

#undef CONST_COMPILE
#undef IS_LITERAL
#undef IS_COMPILABLE
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
