#include"fklffidll.h"
#include"fklffimem.h"
#include<pthread.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/fklni.h>

static pthread_rwlock_t GlobSharedObjsLock=PTHREAD_RWLOCK_INITIALIZER;

typedef struct FklFfiSharedObjNode
{
	FklFfidllHandle dll;
	struct FklFfiSharedObjNode* next;
}FklFfiSharedObjNode;

static FklFfiSharedObjNode* GlobSharedObjs=NULL;

void fklFfiAddSharedObj(FklFfidllHandle handle)
{
	FklFfiSharedObjNode* node=(FklFfiSharedObjNode*)malloc(sizeof(FklFfiSharedObjNode));
	FKL_ASSERT(node);
	node->dll=handle;
	pthread_rwlock_wrlock(&GlobSharedObjsLock);
	node->next=GlobSharedObjs;
	GlobSharedObjs=node;
	pthread_rwlock_unlock(&GlobSharedObjsLock);
}

void fklFfiFreeAllSharedObj(void)
{
	FklFfiSharedObjNode* head=GlobSharedObjs;
	pthread_rwlock_wrlock(&GlobSharedObjsLock);
	GlobSharedObjs=NULL;
	pthread_rwlock_unlock(&GlobSharedObjsLock);
	while(head)
	{
		FklFfiSharedObjNode* prev=head;
		head=head->next;
#ifdef _WIN32
		FreeLibrary(prev->dll);
#else
		dlclose(prev->dll);
#endif
		free(prev);
	}
}

void fklFfiFreeProc(FklFfiProc* t)
{
	free(t->atypes);
	free(t);
}

static void _ffi_proc_atomic_finalizer(void* p)
{
	fklFfiFreeProc(p);
}

static void _ffi_proc_print(void* p,FILE* fp)
{
	FklFfiProc* f=p;
	fprintf(fp,"#<ffi-proc:");
	fklPrintString(fklGetGlobSymbolWithId(f->sid)->symbol,fp);
	fputc('>',fp);
}

static pthread_mutex_t GPrepCifLock=PTHREAD_MUTEX_INITIALIZER;
static ffi_type* NativeFFITypeList[]=
{
	&ffi_type_void,
	&ffi_type_sshort,
	&ffi_type_sint,
	&ffi_type_ushort,
	&ffi_type_uint,
	&ffi_type_slong,
	&ffi_type_ulong,
	&ffi_type_slong,//long long
	&ffi_type_ulong,//unsigned long long
	&ffi_type_slong,//ptrdiff_t
	&ffi_type_ulong,//size_t
	&ffi_type_slong,//ssize_t
	&ffi_type_schar,//char
	&ffi_type_sint32,//wchar_t
	&ffi_type_float,
	&ffi_type_double,
	&ffi_type_sint8,
	&ffi_type_uint8,
	&ffi_type_sint16,
	&ffi_type_uint16,
	&ffi_type_sint32,
	&ffi_type_uint32,
	&ffi_type_sint64,
	&ffi_type_uint64,
	&ffi_type_slong,//intptr_t
	&ffi_type_ulong,//uintptr_t
	&ffi_type_pointer,//void*
};

void fklFfiPrepFFIcif(ffi_cif* cif,int argc,ffi_type** atypes,ffi_type* rtype)
{
	pthread_mutex_lock(&GPrepCifLock);
	ffi_status r=ffi_prep_cif(cif,FFI_DEFAULT_ABI,argc,rtype,atypes);
	pthread_mutex_unlock(&GPrepCifLock);
	FKL_ASSERT(r==FFI_OK);
}

ffi_type* fklFfiGetFfiType(FklTypeId_t type)
{
	if(type>FKL_FFI_VPTR)
		type=FKL_FFI_VPTR;
	return NativeFFITypeList[type];
}

static void _ffi_proc_invoke(void* ptr,FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklFfiProc* proc=ptr;
	FklTypeId_t type=proc->type;
	FklVMrunnable* curR=exe->rhead;
	FklDefFuncType* ft=(FklDefFuncType*)FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(type).all);
	uint32_t anum=ft->anum;
	uint32_t i=0;
	FklTypeId_t rtype=ft->rtype;
	FklTypeId_t* atypes=ft->atypes;
	FklVMvalue** args=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*anum);
	FKL_ASSERT(args);
	for(i=0;i<anum;i++)
	{
		FklVMvalue* v=fklNiGetArg(&ap,stack);
		if(v==NULL)
		{
			free(args);
			FKL_RAISE_BUILTIN_ERROR(fklGetGlobSymbolWithId(proc->sid)->symbol,FKL_TOOFEWARG,curR,exe);
		}
		args[i]=v;
	}
	if(fklNiResBp(&ap,stack))
	{
		free(args);
		FKL_RAISE_BUILTIN_ERROR(fklGetGlobSymbolWithId(proc->sid)->symbol,FKL_TOOMANYARG,curR,exe);
	}
	void** pArgs=(void**)malloc(sizeof(void*)*anum);
	FklVMudata** udataList=(FklVMudata**)malloc(sizeof(FklVMudata*)*anum);
	FKL_ASSERT(udataList);
	FKL_ASSERT(pArgs);
	for(i=0;i<anum;i++)
	{
		FklVMudata* ud=fklFfiNewMemUd(atypes[i],fklFfiGetTypeSizeWithTypeId(atypes[i]),NULL);
		if(fklFfiSetMemForProc(ud,args[i]))
		{
			for(uint32_t j=0;j<i;j++)
			{
				ud->t->__finalizer(ud->data);
				fklFreeVMudata(ud);
				FklVMudata* tud=udataList[i];
				tud->t->__finalizer(tud->data);
				fklFreeVMudata(tud);
			}
			free(args);
			free(pArgs);
			FKL_RAISE_BUILTIN_ERROR(fklGetGlobSymbolWithId(proc->sid)->symbol,FKL_WRONGARG,curR,exe);
		}
		FklFfiMem* mem=ud->data;
		if(mem->type==FKL_FFI_FILE_P||mem->type==FKL_FFI_STRING||fklFfiIsArrayTypeId(mem->type))
			pArgs[i]=&mem->mem;
		else
			pArgs[i]=mem->mem;
		udataList[i]=ud;
	}
	if(rtype==0)
	{
		void* retval=NULL;
		ffi_call(&proc->cif,proc->func,&retval,pArgs);
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
	{
		FklVMudata* ud=fklFfiNewMemUd(rtype,fklFfiGetTypeSizeWithTypeId(rtype),NULL);
		void* retval=((FklFfiMem*)ud->data)->mem;
		FKL_ASSERT(retval);
		ffi_call(&proc->cif,proc->func,retval,pArgs);
		fklNiReturn(fklNewVMvalueToStack(FKL_USERDATA,ud,stack,exe->heap),&ap,stack);
	}
	for(i=0;i<anum;i++)
	{
		FklVMudata* ud=udataList[i];
		ud->t->__finalizer(ud->data);
		fklFreeVMudata(ud);
	}
	free(udataList);
	free(pArgs);
	free(args);
	fklNiEnd(&ap,stack);
}

extern int _mem_equal(const FklVMudata* a,const FklVMudata* b);
static FklVMudMethodTable FfiProcMethodTable=
{
	.__princ=_ffi_proc_print,
	.__prin1=_ffi_proc_print,
	.__finalizer=_ffi_proc_atomic_finalizer,
	.__equal=_mem_equal,
	.__call=_ffi_proc_invoke,
	.__write=NULL,
	.__atomic=NULL,
	.__append=NULL,
	.__copy=NULL,
};

int fklFfiIsProc(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.ud->type==fklFfiGetFfiMemUdSid()&&p->u.ud->t==&FfiProcMethodTable;
}

int fklFfiIsValidFunctionTypeId(FklSid_t id)
{
	FklDefFuncType* ft=(FklDefFuncType*)FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(id).all);
	uint32_t anum=ft->anum;
	FklTypeId_t* atypes=ft->atypes;
	FklTypeId_t rtype=ft->rtype;
	uint32_t i=0;
	for(;i<anum;i++)
	{
		if(!fklFfiIsArrayTypeId(atypes[i])&&fklFfiGetTypeSizeWithTypeId(atypes[i])>sizeof(void*))
			return 0;
	}
	if(rtype&&!fklFfiIsArrayTypeId(rtype)&&fklFfiGetTypeSizeWithTypeId(rtype)>sizeof(void*))
		return 0;
	return 1;
}

FklFfiProc* fklFfiNewProc(FklTypeId_t type,void* func,FklSid_t sid)
{
	FklFfiProc* tmp=(FklFfiProc*)malloc(sizeof(FklFfiProc));
	FKL_ASSERT(tmp);
	tmp->type=type;
	tmp->func=func;
	tmp->sid=sid;
	FklDefFuncType* ft=(FklDefFuncType*)FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(type).all);
	uint32_t anum=ft->anum;
	FklTypeId_t* atypes=ft->atypes;
	ffi_type** ffiAtypes=(ffi_type**)malloc(sizeof(ffi_type*)*anum);
	FKL_ASSERT(ffiAtypes);
	uint32_t i=0;
	for(;i<anum;i++)
		ffiAtypes[i]=fklFfiGetFfiType(atypes[i]);
	FklTypeId_t rtype=ft->rtype;
	ffi_type* ffiRtype=NULL;
	ffiRtype=fklFfiGetFfiType(rtype);
	fklFfiPrepFFIcif(&tmp->cif,anum,ffiAtypes,ffiRtype);
	tmp->atypes=ffiAtypes;
	return tmp;
}

FklVMudata* fklFfiNewProcUd(FklTypeId_t type,const char* cStr)
{
	pthread_rwlock_rdlock(&GlobSharedObjsLock);
	void* address=NULL;
	for(FklFfiSharedObjNode* head=GlobSharedObjs;head;head=head->next)
	{
		address=fklGetAddress(cStr,head->dll);
		if(address)
			break;
	}
	pthread_rwlock_unlock(&GlobSharedObjsLock);
	if(!address)
		return NULL;
	return fklNewVMudata(fklFfiGetFfiMemUdSid(),&FfiProcMethodTable,fklFfiNewProc(type,address,fklAddSymbolToGlobCstr(cStr)->id),fklFfiGetRel());
}
