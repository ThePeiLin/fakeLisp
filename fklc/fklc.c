#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<string.h>
#include"fbc.h"
#include"flnt.h"
#include"fsym.h"
#define ARGL FklVM* exe,FklVMvalue* dll,FklVMvalue* pd

#define FKLC_RAISE_ERROR(WHO,ERRORTYPE,EXE,PD) do{\
	FklString* errorMessage=fklcGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR\
			,fklCreateVMerrorCstr((WHO)\
				,fklcGetErrorType(ERRORTYPE,(PD)->fklcErrorTypeId)\
				,errorMessage)\
			,(EXE));\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)


#define PREDICATE(condtion,err_infor) {\
	FKL_NI_BEGIN(exe);\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,exe);\
	if(!val)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,exe);\
	if(condtion)\
	fklNiReturn(FKL_VM_TRUE,&ap,stack);\
	else\
	fklNiReturn(FKL_VM_NIL,&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void fklc_fbc_p(ARGL) PREDICATE(fklcIsFbc(val),"fklc.fbc?")

#undef PREDICATE

#define IS_LITERAL(V) ((V)==FKL_VM_NIL||fklIsVMnumber(V)||FKL_IS_CHR(V)||FKL_IS_STR(V)||FKL_IS_BYTEVECTOR(V)||FKL_IS_SYM(V))

#define IS_COMPILABLE(V) (IS_LITERAL(V)||FKL_IS_PAIR(V)||FKL_IS_BOX(V)||FKL_IS_VECTOR(V))

#define COMPILE_INTEGER(V) (FKL_IS_I32(V)?\
		fklCreatePushI32ByteCode(FKL_GET_I32(V)):\
		FKL_IS_I64(V)?\
		fklCreatePushI64ByteCode((V)->u.i64):\
		fklCreatePushBigIntByteCode((V)->u.bigInt))

#define COMPILE_NUMBER(V) (fklIsInt(V)?COMPILE_INTEGER(V):fklCreatePushF64ByteCode((V)->u.f64))

#define COMPILE_LITERAL(V) ((V)==FKL_VM_NIL?\
		fklCreatePushNilByteCode():\
		fklIsVMnumber(V)?\
		COMPILE_NUMBER(V):\
		FKL_IS_CHR(V)?\
		fklCreatePushCharByteCode(FKL_GET_CHR(V)):\
		FKL_IS_STR(V)?\
		fklCreatePushStrByteCode((V)->u.str):\
		FKL_IS_SYM(V)?\
		fklCreatePushSidByteCode(FKL_GET_SYM(V)):\
		fklCreatePushBvecByteCode((V)->u.bvec))

#define CONST_COMPILE(ERR_INFO,ARG,P,BC) {\
	FKL_NI_BEGIN(exe);\
	FklVMvalue* ARG=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOMANYARG,exe);\
	if(!ARG)\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOFEWARG,exe);\
	if(!P(ARG))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklcCreateFbcUd(BC,dll,pd),exe),&ap,stack);\
	fklNiEnd(&ap,stack);\
}

void fklc_make_push_nil(ARGL)
{
	FKL_NI_BEGIN(exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-push-nil",FKL_ERR_TOOMANYARG,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklcCreateFbcUd(fklCreatePushNilByteCode(),dll,pd),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_fbc_to_bytevector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fbc=fklNiGetArg(&ap,stack);
	if(!fbc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(fbc,fklcIsFbc,"fklc.fbc->bytevector",exe);
	FklByteCode* bc=fbc->u.ud->data;
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(bc->size,bc->code),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_bytevector_to_fbc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* bv=fklNiGetArg(&ap,stack);
	if(!bv)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.fbc->bytevector",FKL_ERR_TOOMANYARG,exe);
	FKL_NI_CHECK_TYPE(bv,FKL_IS_BYTEVECTOR,"fklc.fbc->bytevector",exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklcCreateFbcUd(fklCreateByteCodeAndInit(bv->u.bvec->size,bv->u.bvec->ptr),dll,pd),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_compile_i32(ARGL) CONST_COMPILE("fklc.compile-i32",i_32,FKL_IS_I32,fklCreatePushI32ByteCode(FKL_GET_I32(i_32)))
void fklc_compile_i64(ARGL) CONST_COMPILE("fklc.compile-i64",i_64,FKL_IS_I64,fklCreatePushI64ByteCode(i_64->u.i64))
void fklc_compile_char(ARGL) CONST_COMPILE("fklc.compile-char",chr,FKL_IS_CHR,fklCreatePushCharByteCode(FKL_GET_CHR(chr)))
void fklc_compile_symbol(ARGL) CONST_COMPILE("fklc.compile-symbol",sym,FKL_IS_SYM,fklCreatePushSidByteCode(fklcGetSymbolIdWithOuterSymbolId(FKL_GET_SYM(sym))))
void fklc_compile_big_int(ARGL) CONST_COMPILE("fklc.compile-big-int",big_int,FKL_IS_BIG_INT,fklCreatePushBigIntByteCode(big_int->u.bigInt))
void fklc_compile_f64(ARGL) CONST_COMPILE("fklc.compile-f64",f_64,FKL_IS_F64,fklCreatePushF64ByteCode(f_64->u.f64))
void fklc_compile_string(ARGL) CONST_COMPILE("fklc.compile-string",str,FKL_IS_STR,fklCreatePushStrByteCode(str->u.str))
void fklc_compile_bytevector(ARGL) CONST_COMPILE("fklc.compile-bytevector",bvec,FKL_IS_BYTEVECTOR,fklCreatePushBvecByteCode(bvec->u.bvec))
void fklc_compile_integer(ARGL) CONST_COMPILE("fklc.compile-integer",integer,fklIsInt,COMPILE_INTEGER(integer))
void fklc_compile_number(ARGL) CONST_COMPILE("fklc.compile-number",number,fklIsVMnumber,COMPILE_NUMBER(number))
void fklc_compile_atom_literal(ARGL) CONST_COMPILE("fklc.compile-atom-literal",literal,IS_LITERAL,COMPILE_LITERAL(literal))
void fklc_compile_obj(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.compile-obj",FKL_ERR_TOOMANYARG,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.compile-obj",FKL_ERR_TOOFEWARG,exe);
	if(!IS_COMPILABLE(obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.compile-obj",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklByteCode* bc=fklcCreatePushObjByteCode(obj,exe->symbolTable,pd);
	FklFklcPublicData* publicData=pd->u.ud->data;
	if(!bc)
		FKLC_RAISE_ERROR("fklc.compile-obj",FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR,exe,publicData);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklcCreateFbcUd(bc,dll,pd),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_add_symbol(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.add-symbol!",FKL_ERR_TOOMANYARG,exe);
	if(!str)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.add-symbol!",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(str,FKL_IS_STR,"fklc.add-symbol!",exe);
	FklFklcPublicData* publicData=pd->u.ud->data;
	fklNiReturn(fklMakeVMint(fklAddSymbol(str->u.str,publicData->outerSymbolTable)->id,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void fklc_make_fbc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* size=fklNiGetArg(&ap,stack);
	if(!size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-fbc",FKL_ERR_TOOFEWARG,exe);
	FKL_NI_CHECK_TYPE(size,fklIsInt,"fklc.make-fbc",exe);
	FklVMvalue* content=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-fbc",FKL_ERR_TOOMANYARG,exe);
	if(fklVMnumberLt0(size))
		FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-fbc",FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	size_t len=fklGetUint(size);
	FklByteCode* bc=fklCreateByteCode(len);
	uint8_t c=0;
	if(content)
	{
		if(!FKL_IS_CHR(content)&&!fklIsInt(content))
			FKL_RAISE_BUILTIN_ERROR_CSTR("fklc.make-fbc",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		c=fklGetInt(content);
	}
	memset(bc->code,c,len);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklcCreateFbcUd(bc,dll,pd),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

#undef CONST_COMPILE
#undef IS_LITERAL
#undef IS_COMPILABLE

static void fklc_pdmt_finalizer(void* data)
{
	FklFklcPublicData* pd=data;
	fklDestroySymbolTable(pd->outerSymbolTable);
	free(data);
}

static const FklVMudMethodTable fklc_pdmt=
{
	.__finalizer=fklc_pdmt_finalizer,
	.__append=NULL,
	.__atomic=NULL,
	.__call=NULL,
	.__cmp=NULL,
	.__copy=NULL,
	.__setq_hook=NULL,
	.__to_string=NULL,
	.__length=NULL,
	.__hash=NULL,
	.__prin1=NULL,
	.__princ=NULL,
	.__write=NULL,
};

static FklFklcPublicData* createFklcPublicData(FklSymbolTable* table)
{
	FklFklcPublicData* r=(FklFklcPublicData*)malloc(sizeof(FklFklcPublicData));
	FKL_ASSERT(r);
	r->bcUdSid=fklAddSymbolCstr("fbc",table)->id;
	r->outerSymbolTable=fklCreateSymbolTable();
	fklFklcInitErrorTypeId(r->fklcErrorTypeId,table);
	return r;
}

void _fklInit(FklVMdll* dll,FklVM* exe)
{
	FklFklcPublicData* pd=createFklcPublicData(exe->symbolTable);
	FklVMudata* pdud=fklCreateVMudata(0,&fklc_pdmt,pd,NULL,NULL);
	FklVMvalue* ud=fklCreateVMvalueToStack(FKL_TYPE_USERDATA,pdud,exe);
	fklSetRef(&dll->pd,ud,exe->gc);
}
