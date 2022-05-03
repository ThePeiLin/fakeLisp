#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include"fklffitype.h"
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
}

void FKL_ffi_delete(ARGL)
{
}

void FKL_ffi_sizeof(ARGL)
{
}

void FKL_ffi_typedef(ARGL)
{
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
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("ffi.load",path,1,FKL_LOADDLLFAILD,r,exe);
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
	fklFfiInitGlobNativeTypes();
}

void _fklUninit(void)
{
	fklFfiFreeGlobDefTypeTable();
	fklFfiFreeGlobTypeList();
	fklFfiFreeAllSharedObj();
}
#undef ARGL
