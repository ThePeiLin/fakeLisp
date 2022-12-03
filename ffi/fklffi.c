#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include<string.h>
#include"fklffitype.h"
#include"fklffimem.h"
#include"fklffidll.h"
#define ARGL FklVM* exe,FklVMvalue* rel,FklVMvalue* pd

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

void ffi_mem_p(ARGL) PREDICATE(fklFfiIsMem(val),"ffi.mem?")

#undef PREDICATE

void ffi_null_p(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(val))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_ERR_TOOFEWARG,exe);
	if(fklFfiIsNull(val->u.ud->data,val->u.ud->pd))
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_new(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* atomic=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.create",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.create",FKL_ERR_TOOMANYARG,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(atomic
				&&!FKL_IS_SYM(atomic)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.create",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiPublicData* publicData=pd->u.ud->data;
	FklTypeId_t id=fklFfiGenTypeId(typedeclare,publicData);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.create",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
	FklVMudata* mem=NULL;
	if(id==FKL_FFI_TYPE_FILE_P||id==FKL_FFI_TYPE_STRING)
		mem=fklFfiCreateMemRefUd(id,NULL,rel,pd);
	else
	{
		size_t size=fklFfiGetTypeSize(fklFfiLockAndGetTypeUnion(id,publicData));
		mem=fklFfiCreateMemUd(id,size,atomic,rel,pd);
	}
	if(!mem)
		FKL_FFI_RAISE_ERROR("ffi.create",FKL_FFI_ERR_INVALID_MEM_MODE,exe,publicData);
	if(id==FKL_FFI_TYPE_FILE_P||id==FKL_FFI_TYPE_STRING)
	{
		mem->t->__finalizer(mem->data);
		FklFfiMem* m=mem->data;
		free(mem);
		free(m->mem);
		m->mem=NULL;
	}
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,mem,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_delete(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(mem))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiMem* m=mem->u.ud->data;
	free(m->mem);
	m->mem=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_sizeof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_ERR_TOOMANYARG,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare)&&!fklFfiIsMem(typedeclare))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiPublicData* publicData=pd->u.ud->data;
	if(fklFfiIsMem(typedeclare))
	{
		FklFfiMem* m=typedeclare->u.ud->data;
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSize(fklFfiLockAndGetTypeUnion(m->type,publicData)),exe),&ap,stack);
	}
	else
	{
		FklTypeId_t id=fklFfiGenTypeId(typedeclare,publicData);
		if(!id)
			FKL_FFI_RAISE_ERROR("ffi.sizeof",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSize(fklFfiLockAndGetTypeUnion(id,publicData)),exe),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void ffi_alignof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_ERR_TOOMANYARG,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare)&&!fklFfiIsMem(typedeclare))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiPublicData* publicData=pd->u.ud->data;
	if(fklFfiIsMem(typedeclare))
	{
		FklFfiMem* m=typedeclare->u.ud->data;
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSize(fklFfiLockAndGetTypeUnion(m->type,publicData)),exe),&ap,stack);
	}
	else
	{
		FklTypeId_t id=fklFfiGenTypeId(typedeclare,publicData);
		if(!id)
			FKL_FFI_RAISE_ERROR("ffi.alignof",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
		fklNiReturn(fklMakeVMint(fklFfiGetTypeAlign(fklFfiLockAndGetTypeUnion(id,publicData)),exe),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void ffi_typedef(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* typename=fklNiGetArg(&ap,stack);
	if(!typedeclare||!typename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_ERR_TOOMANYARG,exe);
	if(!FKL_IS_SYM(typename)||(!FKL_IS_PAIR(typedeclare)&&!FKL_IS_SYM(typedeclare)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklSid_t typenameId=FKL_GET_SYM(typename);
	FklFfiPublicData* publicData=pd->u.ud->data;
	if(fklFfiIsNativeTypeName(typenameId,publicData))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_ERR_INVALID_TYPENAME,exe,publicData);
	if(!fklFfiTypedef(typedeclare,typenameId,publicData))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
	fklNiReturn(typename,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_load(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vpath=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_TOOMANYARG,exe);
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_TOOFEWARG,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	char* path=fklCharBufToCstr(vpath->u.str->str,vpath->u.str->size);
	FklVMdllHandle handle=fklLoadDll(path);
	if(!handle)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("ffi.load",path,1,FKL_ERR_LOADDLLFAILD,exe);
	free(path);
	FklFfiPublicData* publicData=pd->u.ud->data;
	fklFfiAddSharedObj(handle,publicData);
	fklNiReturn(vpath,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_ref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	FklVMvalue* selector=fklNiGetArg(&ap,stack);
	FklVMvalue* index=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(mem)||(selector&&!FKL_IS_SYM(selector)&&selector!=FKL_VM_NIL)||(index&&!fklIsInt(index)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMudata* ref=fklFfiCreateMemRefUdWithSI(mem->u.ud->data,selector,index,rel,pd);
	FklFfiPublicData* publicData=pd->u.ud->data;
	if(!ref)
		FKL_FFI_RAISE_ERROR("ffi.ref",FKL_FFI_ERR_INVALID_SELECTOR,exe,publicData);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA
				,ref
				,exe)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void ffi_clear(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(mem))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiMem* ptr=mem->u.ud->data;
	if(ptr->type==FKL_FFI_TYPE_STRING)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiPublicData* publicData=pd->u.ud->data;
	FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(ptr->type,publicData);
	memset(ptr->mem,0,fklFfiGetTypeSize(tu));
	fklNiReturn(mem
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void ffi_cast_ref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem||!type)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.cast-ref",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.cast-ref",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(mem)||(!FKL_IS_PAIR(type)&&!FKL_IS_SYM(type)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.cast-ref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiPublicData* publicData=pd->u.ud->data;
	FklTypeId_t id=fklFfiGenTypeId(type,publicData);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.cast-ref",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
	FklVMudata* ref=fklFfiCreateMemRefUd(id,((FklFfiMem*)mem->u.ud->data)->mem,rel,pd);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA
				,ref
				,exe)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void ffi_set(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* ref=fklNiGetArg(&ap,stack);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem||!ref)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(ref)
			||(!fklFfiIsMem(mem)&&!fklFfiIsCastableVMvalueType(mem)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklFfiPublicData* publicData=pd->u.ud->data;
	if(fklFfiSetMem(ref->u.ud,ref->u.ud->data,mem,exe->symbolTable))
		FKL_FFI_RAISE_ERROR("ffi.set",FKL_FFI_ERR_INVALID_ASSIGN,exe,publicData);
	fklNiReturn(mem,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_mem(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsCastableVMvalueType(val))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklFfiCastVMvalueIntoMem(val,rel,pd,exe->symbolTable),exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_val(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_ERR_TOOMANYARG,exe);
	if(!fklFfiIsMem(mem)||!fklFfiIsValuableMem(mem->u.ud->data,mem->u.ud->pd))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(fklFfiCreateVMvalue(mem->u.ud->data,exe,mem->u.ud->pd),&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_proc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!val||!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_ERR_TOOFEWARG,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_ERR_TOOMANYARG,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(val
				&&!FKL_IS_STR(val)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	FklFfiPublicData* publicData=pd->u.ud->data;
	FklTypeId_t id=fklFfiGenTypeId(typedeclare,publicData);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.proc",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
	else
	{
		FklDefTypeUnion tu=fklFfiLockAndGetTypeUnion(id,publicData);
		if(!fklFfiIsFunctionType(tu)||!fklFfiIsValidFunctionType(tu,publicData))
			FKL_FFI_RAISE_ERROR("ffi.proc",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe,publicData);
	}
	char* cStr=fklCharBufToCstr(val->u.str->str,val->u.str->size);
	FklVMudata* func=fklFfiCreateProcUd(id,cStr,rel,pd,exe->symbolTable);
	if(!func)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("ffi.proc",cStr,1,FKL_ERR_INVALIDSYMBOL,exe);
	free(cStr);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,func,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static FklFfiPublicData* createFfiPd(void)
{
	FklFfiPublicData* pd=(FklFfiPublicData*)malloc(sizeof(FklFfiPublicData));
	FKL_ASSERT(pd);
	return pd;
}

static void ffi_pd_finalizer(void* data)
{
	FklFfiPublicData* pd=data;
	fklFfiDestroyGlobDefTypeTable(pd);
	fklFfiDestroyGlobTypeList(pd);
	fklFfiDestroyAllSharedObj(pd);
	free(data);
}

static FklVMudMethodTable pdtable=
{
	.__append=NULL,
	.__atomic=NULL,
	.__cmp=NULL,
	.__copy=NULL,
	.__equal=NULL,
	.__prin1=NULL,
	.__princ=NULL,
	.__finalizer=ffi_pd_finalizer,
};

void _fklInit(FklVMdll* rel,FklVM* exe)
{
	FklVMgc* gc=exe->gc;
	FklFfiPublicData* pd=createFfiPd();
	fklFfiMemInit(pd,exe->symbolTable);
	fklFfiInitGlobNativeTypes(pd,exe->symbolTable);
	fklFfiInitTypedefSymbol(pd,exe->symbolTable);
	fklInitErrorTypes(pd,exe->symbolTable);
	fklFfiInitSharedObj(pd);
	FklVMvalue* ud=fklCreateVMvalueToStack(FKL_TYPE_USERDATA,fklCreateVMudata(0,&pdtable,pd,FKL_VM_NIL,FKL_VM_NIL),exe);
	fklSetRef(&rel->pd,ud,gc);
}
#undef ARGL
