#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include"fklffitype.h"
#include"fklffimem.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

static pthread_rwlock_t GlobSharedObjsLock=PTHREAD_RWLOCK_INITIALIZER;

typedef struct FklSharedObjNode
{
	FklVMdllHandle dll;
	struct FklSharedObjNode* next;
}FklSharedObjNode;

static FklSharedObjNode* GlobSharedObjs=NULL;

static void fklFfiAddSharedObj(FklVMdllHandle handle)
{
	FklSharedObjNode* node=(FklSharedObjNode*)malloc(sizeof(FklSharedObjNode));
	FKL_ASSERT(node,__func__);
	node->dll=handle;
	pthread_rwlock_wrlock(&GlobSharedObjsLock);
	node->next=GlobSharedObjs;
	GlobSharedObjs=node;
	pthread_rwlock_unlock(&GlobSharedObjsLock);
}

static void fklFfiFreeAllSharedObj(void)
{
	FklSharedObjNode* head=GlobSharedObjs;
	pthread_rwlock_wrlock(&GlobSharedObjsLock);
	GlobSharedObjs=NULL;
	pthread_rwlock_unlock(&GlobSharedObjsLock);
	while(head)
	{
		FklSharedObjNode* prev=head;
		head=head->next;
#ifdef _WIN32
		FreeLibrary(prev->dll);
#else
		dlclose(prev->dll);
#endif
		free(prev);
	}
}

void FKL_ffi_new(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* atomic=fklNiGetArg(&ap,stack);
	if(!typedeclare)
		FKL_RAISE_BUILTIN_ERROR("ffi.new",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.new",FKL_TOOMANYARG,exe->rhead,exe);
	if((!FKL_IS_SYM(typedeclare)
				&&!FKL_IS_PAIR(typedeclare))
			||(atomic
				&&!FKL_IS_SYM(atomic)))
		FKL_RAISE_BUILTIN_ERROR("ffi.new",FKL_WRONGARG,exe->rhead,exe);
	FklTypeId_t id=fklFfiGenTypeId(typedeclare);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.new",FKL_FFI_INVALID_TYPEDECLARE,exe);
	size_t size=fklFfiGetTypeSizeWithTypeId(id);
	FklVMudata* mem=fklFfiNewMemUd(id,size,atomic);
	if(!mem)
		FKL_FFI_RAISE_ERROR("ffi.new",FKL_FFI_INVALID_MEM_MODE,exe);
	fklNiReturn(fklNiNewVMvalue(FKL_USERDATA,mem,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_delete(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* mem=fklNiGetArg(&ap,stack);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR("ffi.delete",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.delete",FKL_TOOMANYARG,exe->rhead,exe);
	if(!fklFfiIsMem(mem))
		FKL_RAISE_BUILTIN_ERROR("ffi.delete",FKL_WRONGARG,exe->rhead,exe);
	FklFfiMem* m=mem->u.p->mem;
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
		FKL_RAISE_BUILTIN_ERROR("ffi.sizeof",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.sizeof",FKL_TOOMANYARG,exe->rhead,exe);
	if(!FKL_IS_SYM(typedeclare)&&!FKL_IS_PAIR(typedeclare))
		FKL_RAISE_BUILTIN_ERROR("ffi.sizeof",FKL_WRONGARG,exe->rhead,exe);
	FklTypeId_t id=fklFfiGenTypeId(typedeclare);
	if(!id)
		FKL_FFI_RAISE_ERROR("ffi.sizeof",FKL_FFI_INVALID_TYPEDECLARE,exe);
	fklNiReturn(fklMakeVMint(fklFfiGetTypeSizeWithTypeId(id),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_typedef(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* typedeclare=fklNiGetArg(&ap,stack);
	FklVMvalue* typename=fklNiGetArg(&ap,stack);
	if(!typedeclare||!typename)
		FKL_RAISE_BUILTIN_ERROR("ffi.typedef",FKL_TOOFEWARG,exe->rhead,exe);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.typedef",FKL_TOOMANYARG,exe->rhead,exe);
	if(!FKL_IS_SYM(typename)||(!FKL_IS_PAIR(typedeclare)&&!FKL_IS_SYM(typedeclare)))
		FKL_RAISE_BUILTIN_ERROR("ffi.typedef",FKL_WRONGARG,exe->rhead,exe);
	FklSid_t typenameId=FKL_GET_SYM(typename);
	if(fklFfiIsNativeTypeName(typenameId))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_INVALID_TYPENAME,exe);
	if(!fklFfiTypedef(typedeclare,typenameId))
		FKL_FFI_RAISE_ERROR("ffi.typedef",FKL_FFI_INVALID_TYPEDECLARE,exe);
	fklNiReturn(typename,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_call(ARGL)
{
}

void FKL_ffi_load(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* vpath=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.load",FKL_TOOMANYARG,r,exe);
	if(exe->VMid==-1)
		return;
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR("ffi.load",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR("ffi.load",FKL_WRONGARG,r,exe);
	char* path=fklCharBufToStr(vpath->u.str->str,vpath->u.str->size);
	FklVMdllHandle handle=fklLoadDll(path);
	if(!handle)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("ffi.load",path,1,FKL_LOADDLLFAILD,exe);
	free(path);
	fklFfiAddSharedObj(handle);
	fklNiReturn(vpath,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_ref(ARGL)
{
}

void FKL_ffi_set(ARGL)
{
}

void _fklInit(FklSymbolTable* glob)
{
	fklSetGlobSymbolTable(glob);
	fklFfiMemInit();
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
