#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include<string.h>
#include"fklffitype.h"
#include"fklffimem.h"
#include"fklffidll.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

#define PREDICATE(condtion,err_infor) {\
	FKL_NI_BEGIN(exe);\
	FklVMframe* frame=exe->frames;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
		FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,frame,exe);\
	if(!val)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(err_infor,FKL_ERR_TOOFEWARG,frame,exe);\
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsMem(val))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklFfiIsNull(val->u.ud->data))
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.new",FKL_ERR_TOOFEWARG,exe->frames,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.new",FKL_ERR_TOOMANYARG,exe->frames,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(atomic
				&&!FKL_IS_SYM(atomic)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.new",FKL_ERR_INCORRECT_TYPE_VALUE,exe->frames,exe);
	FklTypeId_t id=fklFfiGenTypeId(typedeclare);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.new",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe);
	FklVMudata* mem=NULL;
	if(id==FKL_FFI_TYPE_FILE_P||id==FKL_FFI_TYPE_STRING)
		mem=fklFfiNewMemRefUd(id,NULL);
	else
	{
		size_t size=fklFfiGetTypeSizeWithTypeId(id);
		mem=fklFfiNewMemUd(id,size,atomic);
	}
	if(!mem)
		FKL_FFI_RAISE_ERROR("ffi.new",FKL_FFI_ERR_INVALID_MEM_MODE,exe);
	if(id==FKL_FFI_TYPE_FILE_P||id==FKL_FFI_TYPE_STRING)
	{
		mem->t->__finalizer(mem->data);
		FklFfiMem* m=mem->data;
		free(mem);
		free(m->mem);
		m->mem=NULL;
	}
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,mem,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_delete(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_ERR_TOOFEWARG,exe->frames,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_ERR_TOOMANYARG,exe->frames,exe);
	if(!fklFfiIsMem(mem))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_ERR_INCORRECT_TYPE_VALUE,exe->frames,exe);
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
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_ERR_TOOFEWARG,exe->frames,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_ERR_TOOMANYARG,exe->frames,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare)&&!fklFfiIsMem(typedeclare))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_ERR_INCORRECT_TYPE_VALUE,exe->frames,exe);
	if(fklFfiIsMem(typedeclare))
	{
		FklFfiMem* m=typedeclare->u.ud->data;
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(m->type),stack,exe->gc),&ap,stack);
	}
	else
	{
		FklTypeId_t id=fklFfiGenTypeId(typedeclare);
		if(!id)
			FKL_FFI_RAISE_ERROR("ffi.sizeof",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe);
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(id),stack,exe->gc),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void ffi_alignof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_ERR_TOOFEWARG,exe->frames,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_ERR_TOOMANYARG,exe->frames,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare)&&!fklFfiIsMem(typedeclare))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_ERR_INCORRECT_TYPE_VALUE,exe->frames,exe);
	if(fklFfiIsMem(typedeclare))
	{
		FklFfiMem* m=typedeclare->u.ud->data;
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(m->type),stack,exe->gc),&ap,stack);
	}
	else
	{
		FklTypeId_t id=fklFfiGenTypeId(typedeclare);
		if(!id)
			FKL_FFI_RAISE_ERROR("ffi.alignof",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe);
		fklNiReturn(fklMakeVMint(fklFfiGetTypeAlignWithTypeId(id),stack,exe->gc),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void ffi_typedef(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* typename=fklNiGetArg(&ap,stack);
	if(!typedeclare||!typename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_ERR_TOOFEWARG,exe->frames,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_ERR_TOOMANYARG,exe->frames,exe);
	if(!FKL_IS_SYM(typename)||(!FKL_IS_PAIR(typedeclare)&&!FKL_IS_SYM(typedeclare)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_ERR_INCORRECT_TYPE_VALUE,exe->frames,exe);
	FklSid_t typenameId=FKL_GET_SYM(typename);
	if(fklFfiIsNativeTypeName(typenameId))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_ERR_INVALID_TYPENAME,exe);
	if(!fklFfiTypedef(typedeclare,typenameId))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe);
	fklNiReturn(typename,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_load(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* vpath=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_TOOMANYARG,frame,exe);
	if(!exe->thrds)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_CANTCREATETHREAD,frame,exe);
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_TOOFEWARG,frame,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	char* path=fklCharBufToCstr(vpath->u.str->str,vpath->u.str->size);
	FklVMdllHandle handle=fklLoadDll(path);
	if(!handle)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("ffi.load",path,1,FKL_ERR_LOADDLLFAILD,exe);
	free(path);
	fklFfiAddSharedObj(handle);
	fklNiReturn(vpath,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_ref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	FklVMvalue* selector=fklNiGetArg(&ap,stack);
	FklVMvalue* index=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsMem(mem)||(selector&&!FKL_IS_SYM(selector)&&selector!=FKL_VM_NIL)||(index&&!fklIsInt(index)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklVMudata* ref=fklFfiNewMemRefUdWithSI(mem->u.ud->data,selector,index);
	if(!ref)
		FKL_FFI_RAISE_ERROR("ffi.ref",FKL_FFI_ERR_INVALID_SELECTOR,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA
				,ref
				,stack
				,exe->gc)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void ffi_clear(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsMem(mem))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklFfiMem* ptr=mem->u.ud->data;
	if(ptr->type==FKL_FFI_TYPE_STRING)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.clear!",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	memset(ptr->mem,0,fklFfiGetTypeSizeWithTypeId(ptr->type));
	fklNiReturn(mem
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void ffi_cast_ref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem||!type)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.cast-ref",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.cast-ref",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsMem(mem)||(!FKL_IS_PAIR(type)&&!FKL_IS_SYM(type)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.cast-ref",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	FklTypeId_t id=fklFfiGenTypeId(type);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.cast-ref",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe);
	FklVMudata* ref=fklFfiNewMemRefUd(id,((FklFfiMem*)mem->u.ud->data)->mem);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA
				,ref
				,stack
				,exe->gc)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void ffi_set(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* ref=fklNiGetArg(&ap,stack);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem||!ref)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsMem(ref)
			||(!fklFfiIsMem(mem)&&!fklFfiIsCastableVMvalueType(mem)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	if(fklFfiSetMem(ref->u.ud->data,mem))
		FKL_FFI_RAISE_ERROR("ffi.set",FKL_FFI_ERR_INVALID_ASSIGN,exe);
	fklNiReturn(mem,&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_mem(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsCastableVMvalueType(val))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,fklFfiCastVMvalueIntoMem(val),stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_val(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_ERR_TOOMANYARG,frame,exe);
	if(!fklFfiIsMem(mem)||!fklFfiIsValuableMem(mem->u.ud->data))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiReturn(fklFfiNewVMvalue(mem->u.ud->data,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void ffi_proc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!val||!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_ERR_TOOFEWARG,frame,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_ERR_TOOMANYARG,frame,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(val
				&&!FKL_IS_STR(val)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_ERR_INCORRECT_TYPE_VALUE,exe->frames,exe);

	FklTypeId_t id=fklFfiGenTypeId(typedeclare);
	if(!id||!fklFfiIsFunctionTypeId(id)||!fklFfiIsValidFunctionTypeId(id))
		FKL_FFI_RAISE_ERROR("ffi.proc",FKL_FFI_ERR_INVALID_TYPEDECLARE,exe);
	char* cStr=fklCharBufToCstr(val->u.str->str,val->u.str->size);
	FklVMudata* func=fklFfiNewProcUd(id,cStr);
	if(!func)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("ffi.proc",cStr,1,FKL_ERR_INVALIDSYMBOL,exe);
	free(cStr);
	fklNiReturn(fklNewVMvalueToStack(FKL_TYPE_USERDATA,func,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
}

void _fklInit(FklSymbolTable* glob,FklVMvalue* rel,FklVMlist* GlobVMs)
{
	fklSetGlobSymbolTable(glob);
	fklSetGlobVMs(GlobVMs);
	fklFfiMemInit(rel);
	fklFfiInitGlobNativeTypes();
	fklFfiInitTypedefSymbol();
}

void _fklUninit(void)
{
	fklFfiFreeGlobDefTypeTable();
	fklFfiFreeGlobTypeList();
	fklFfiFreeAllSharedObj();
}
#undef ARGL
