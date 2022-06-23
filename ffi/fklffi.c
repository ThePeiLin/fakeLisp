#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include"fklffitype.h"
#include"fklffimem.h"
#include"fklffidll.h"
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

void FKL_ffi_mem_p(ARGL) PREDICATE(fklFfiIsMem(val),"ffi.mem?")

#undef PREDICATE

void FKL_ffi_null_p(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_TOOMANYARG,runnable,exe);
	if(!fklFfiIsMem(val))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.null?",FKL_TOOFEWARG,runnable,exe);
	if(fklFfiIsNull(val->u.ud->data))
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_new(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* atomic=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.new",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.new",FKL_TOOMANYARG,exe->rhead,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(atomic
				&&!FKL_IS_SYM(atomic)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.new",FKL_WRONGARG,exe->rhead,exe);
	FklTypeId_t id=fklFfiGenTypeId(typedeclare);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.new",FKL_FFI_INVALID_TYPEDECLARE,exe);
	FklVMudata* mem=NULL;
	if(id==FKL_FFI_FILE_P||id==FKL_FFI_STRING)
		mem=fklFfiNewMemRefUd(id,NULL);
	else
	{
		size_t size=fklFfiGetTypeSizeWithTypeId(id);
		mem=fklFfiNewMemUd(id,size,atomic);
	}
	if(!mem)
		FKL_FFI_RAISE_ERROR("ffi.new",FKL_FFI_INVALID_MEM_MODE,exe);
	if(id==FKL_FFI_FILE_P||id==FKL_FFI_STRING)
	{
		mem->t->__finalizer(mem->data);
		FklFfiMem* m=mem->data;
		free(mem);
		free(m->mem);
		m->mem=NULL;
	}
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA,mem,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_delete(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_TOOMANYARG,exe->rhead,exe);
	if(!fklFfiIsMem(mem))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.delete",FKL_WRONGARG,exe->rhead,exe);
	FklFfiMem* m=mem->u.ud->data;
	free(m->mem);
	m->mem=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_sizeof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_TOOMANYARG,exe->rhead,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare)&&!fklFfiIsMem(typedeclare))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.sizeof",FKL_WRONGARG,exe->rhead,exe);
	if(fklFfiIsMem(typedeclare))
	{
		FklFfiMem* m=typedeclare->u.ud->data;
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(m->type),stack,exe->heap),&ap,stack);
	}
	else
	{
		FklTypeId_t id=fklFfiGenTypeId(typedeclare);
		if(!id)
			FKL_FFI_RAISE_ERROR("ffi.sizeof",FKL_FFI_INVALID_TYPEDECLARE,exe);
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(id),stack,exe->heap),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void FKL_ffi_alignof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_TOOMANYARG,exe->rhead,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare)&&!fklFfiIsMem(typedeclare))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.alignof",FKL_WRONGARG,exe->rhead,exe);
	if(fklFfiIsMem(typedeclare))
	{
		FklFfiMem* m=typedeclare->u.ud->data;
		fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(m->type),stack,exe->heap),&ap,stack);
	}
	else
	{
		FklTypeId_t id=fklFfiGenTypeId(typedeclare);
		if(!id)
			FKL_FFI_RAISE_ERROR("ffi.alignof",FKL_FFI_INVALID_TYPEDECLARE,exe);
		fklNiReturn(fklMakeVMint(fklFfiGetTypeAlignWithTypeId(id),stack,exe->heap),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void FKL_ffi_typedef(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* typename=fklNiGetArg(&ap,stack);
	if(!typedeclare||!typename)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_TOOMANYARG,exe->rhead,exe);
	if(!FKL_IS_SYM(typename)||(!FKL_IS_PAIR(typedeclare)&&!FKL_IS_SYM(typedeclare)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.typedef",FKL_WRONGARG,exe->rhead,exe);
	FklSid_t typenameId=FKL_GET_SYM(typename);
	if(fklFfiIsNativeTypeName(typenameId))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_INVALID_TYPENAME,exe);
	if(!fklFfiTypedef(typedeclare,typenameId))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_INVALID_TYPEDECLARE,exe);
	fklNiReturn(typename,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_load(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* vpath=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_TOOMANYARG,r,exe);
	if(exe->VMid==-1)
		return;
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.load",FKL_WRONGARG,r,exe);
	char* path=fklCharBufToCstr(vpath->u.str->str,vpath->u.str->size);
	FklVMdllHandle handle=fklLoadDll(path);
	if(!handle)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("ffi.load",path,1,FKL_LOADDLLFAILD,exe);
	free(path);
	fklFfiAddSharedObj(handle);
	fklNiReturn(vpath,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_ref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	FklVMvalue* selector=fklNiGetArg(&ap,stack);
	FklVMvalue* index=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_TOOMANYARG,r,exe);
	if(!fklFfiIsMem(mem)||(selector&&!FKL_IS_SYM(selector)&&selector!=FKL_VM_NIL)||(index&&!fklIsInt(index)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.ref",FKL_WRONGARG,r,exe);
	FklVMudata* ref=fklFfiNewMemRefUdWithSI(mem->u.ud->data,selector,index);
	if(!ref)
		FKL_FFI_RAISE_ERROR("ffi.ref",FKL_FFI_INVALID_SELECTOR,exe);
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA
				,ref
				,stack
				,exe->heap)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_set(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* ref=fklNiGetArg(&ap,stack);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem||!ref)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_TOOMANYARG,r,exe);
	if(!fklFfiIsMem(ref)
			||(!fklFfiIsMem(mem)&&!fklFfiIsCastableVMvalueType(mem)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.set",FKL_WRONGARG,r,exe);
	if(fklFfiSetMem(ref->u.ud->data,mem))
		FKL_FFI_RAISE_ERROR("ffi.set",FKL_FFI_INVALID_ASSIGN,exe);
	fklNiReturn(mem,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_mem(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(!val)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_TOOMANYARG,runnable,exe);
	if(!fklFfiIsCastableVMvalueType(val))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.mem",FKL_WRONGARG,runnable,exe);
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA,fklFfiCastVMvalueIntoMem(val),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_val(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_TOOFEWARG,runnable,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_TOOMANYARG,runnable,exe);
	if(!fklFfiIsMem(mem)||!fklFfiIsValuableMem(mem->u.ud->data))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.val",FKL_WRONGARG,runnable,exe);
	fklNiReturn(fklFfiNewVMvalue(mem->u.ud->data,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_proc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	if(!val||!typedeclare)
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_TOOFEWARG,r,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_TOOMANYARG,r,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(val
				&&!FKL_IS_STR(val)))
		FKL_RAISE_BUILTIN_ERROR_CSTR("ffi.proc",FKL_WRONGARG,exe->rhead,exe);

	FklTypeId_t id=fklFfiGenTypeId(typedeclare);
	if(!id||!fklFfiIsFunctionTypeId(id)||!fklFfiIsValidFunctionTypeId(id))
		FKL_FFI_RAISE_ERROR("ffi.proc",FKL_FFI_INVALID_TYPEDECLARE,exe);
	char* cStr=fklCharBufToCstr(val->u.str->str,val->u.str->size);
	FklVMudata* func=fklFfiNewProcUd(id,cStr);
	if(!func)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("ffi.proc",cStr,1,FKL_INVALIDSYMBOL,exe);
	free(cStr);
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA,func,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void _fklInit(FklSymbolTable* glob,FklVMvalue* rel)
{
	fklSetGlobSymbolTable(glob);
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
