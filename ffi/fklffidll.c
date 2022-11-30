#include"fklffidll.h"
#include"fklffimem.h"
#include<pthread.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/fklni.h>

void fklFfiAddSharedObj(FklFfidllHandle handle,FklFfiPublicData* pd)
{
	FklFfiSharedObjNode* node=(FklFfiSharedObjNode*)malloc(sizeof(FklFfiSharedObjNode));
	FKL_ASSERT(node);
	node->dll=handle;
	pthread_rwlock_wrlock(&pd->sharedObjsLock);
	node->next=pd->sharedObjs;
	pd->sharedObjs=node;
	pthread_rwlock_unlock(&pd->sharedObjsLock);
}

void fklFfiInitSharedObj(FklFfiPublicData* pd)
{
	pthread_rwlock_init(&pd->sharedObjsLock,NULL);
	pd->sharedObjs=NULL;
}

void fklFfiDestroyAllSharedObj(FklFfiPublicData* pd)
{
	FklFfiSharedObjNode* head=pd->sharedObjs;
	pthread_rwlock_wrlock(&pd->sharedObjsLock);
	pd->sharedObjs=NULL;
	pthread_rwlock_unlock(&pd->sharedObjsLock);
	while(head)
	{
		FklFfiSharedObjNode* prev=head;
		head=head->next;
#ifdef _WIN32
		DestroyLibrary(prev->dll);
#else
		dlclose(prev->dll);
#endif
		free(prev);
	}
}

void fklFfiDestroyProc(FklFfiProc* t)
{
	free(t->atypes);
	free(t);
}

static void _ffi_proc_atomic_finalizer(void* p)
{
	fklFfiDestroyProc(p);
}

static void _ffi_proc_print(void* p,FILE* fp)
{
	FklFfiProc* f=p;
	fprintf(fp,"#<ffi-proc ");
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
	if(type>FKL_FFI_TYPE_VPTR)
		type=FKL_FFI_TYPE_VPTR;
	return NativeFFITypeList[type];
}

typedef enum
{
	FFIPROC_READY=0,
	FFIPROC_DONE,
}FfiprocFrameState;

typedef struct
{
	FklVMvalue* proc;
	FfiprocFrameState state;
}FfiprocFrameContext;

static void ffiproc_frame_print_backtrace(void* data[6],FILE* fp)
{
	FfiprocFrameContext* c=(FfiprocFrameContext*)data;
	FklFfiProc* ffiproc=c->proc->u.ud->data;
	if(ffiproc->sid)
	{
		fprintf(fp,"at ffi-proc:");
		fklPrintString(fklGetGlobSymbolWithId(ffiproc->sid)->symbol
				,stderr);
		fputc('\n',fp);
	}
	else
		fputs("at <ffi-proc>\n",fp);
}

static void ffiproc_frame_atomic(void* data[6],FklVMgc* gc)
{
	FfiprocFrameContext* c=(FfiprocFrameContext*)data;
	fklGC_toGrey(c->proc,gc);
}

static void ffiproc_frame_finalizer(void* data[6])
{
	return;
}

static void ffiproc_frame_copy(void* const s[6],void* d[6],FklVM* exe)
{
	FfiprocFrameContext* sc=(FfiprocFrameContext*)s;
	FfiprocFrameContext* dc=(FfiprocFrameContext*)d;
	FklVMgc* gc=exe->gc;
	dc->state=sc->state;
	fklSetRef(&dc->proc,sc->proc,gc);
}

static int ffiproc_frame_end(void* data[6])
{
	FfiprocFrameContext* c=(FfiprocFrameContext*)data;
	return c->state==FFIPROC_DONE;
}

static void _ffi_proc_invoke(FklFfiProc* proc
		,FklVM* exe
		,FklVMvalue* rel)
{
	FKL_NI_BEGIN(exe);
	FklTypeId_t type=proc->type;
	FklFfiPublicData* pd=proc->pd->u.ud->data;
	FklDefFuncType* ft=(FklDefFuncType*)FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(type,pd).all);
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
			FKL_RAISE_BUILTIN_ERROR(fklGetGlobSymbolWithId(proc->sid)->symbol,FKL_ERR_TOOFEWARG,exe);
		}
		args[i]=v;
	}
	if(fklNiResBp(&ap,stack))
	{
		free(args);
		FKL_RAISE_BUILTIN_ERROR(fklGetGlobSymbolWithId(proc->sid)->symbol,FKL_ERR_TOOMANYARG,exe);
	}
	void** pArgs=(void**)malloc(sizeof(void*)*anum);
	FklVMudata** udataList=(FklVMudata**)malloc(sizeof(FklVMudata*)*anum);
	FKL_ASSERT(udataList);
	FKL_ASSERT(pArgs);
	for(i=0;i<anum;i++)
	{
		FklVMudata* ud=fklFfiCreateMemUd(atypes[i],fklFfiGetTypeSizeWithTypeId(atypes[i],pd),NULL,rel,proc->pd);
		if(fklFfiSetMemForProc(ud,args[i]))
		{
			for(uint32_t j=0;j<i;j++)
			{
				FklVMudata* tud=udataList[i];
				tud->t->__finalizer(tud->data);
				fklDestroyVMudata(tud);
			}
			ud->t->__finalizer(ud->data);
			fklDestroyVMudata(ud);
			free(args);
			free(pArgs);
			free(udataList);
			FKL_RAISE_BUILTIN_ERROR(fklGetGlobSymbolWithId(proc->sid)->symbol,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		}
		FklFfiMem* mem=ud->data;
		if(mem->type==FKL_FFI_TYPE_FILE_P||mem->type==FKL_FFI_TYPE_STRING||fklFfiIsArrayTypeId(mem->type,pd))
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
		FklVMudata* ud=fklFfiCreateMemUd(rtype,fklFfiGetTypeSizeWithTypeId(rtype,pd),NULL,rel,proc->pd);
		void* retval=((FklFfiMem*)ud->data)->mem;
		FKL_ASSERT(retval);
		ffi_call(&proc->cif,proc->func,retval,pArgs);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_USERDATA,ud,exe),&ap,stack);
	}
	for(i=0;i<anum;i++)
	{
		FklVMudata* ud=udataList[i];
		ud->t->__finalizer(ud->data);
		fklDestroyVMudata(ud);
	}
	free(udataList);
	free(pArgs);
	free(args);
	fklNiEnd(&ap,stack);
}

static void ffiproc_frame_step(void* data[6],FklVM* exe)
{
	FfiprocFrameContext* c=(FfiprocFrameContext*)data;
	FklFfiProc* ffiproc=c->proc->u.ud->data;
	switch(c->state)
	{
		case FFIPROC_READY:
			c->state=FFIPROC_DONE;
			_ffi_proc_invoke(ffiproc,exe,c->proc->u.ud->rel);
			break;
		case FFIPROC_DONE:
			break;
	}
}

static const FklVMframeContextMethodTable FfiprocContextMethodTable=
{
	.atomic=ffiproc_frame_atomic,
	.finalizer=ffiproc_frame_finalizer,
	.copy=ffiproc_frame_copy,
	.print_backtrace=ffiproc_frame_print_backtrace,
	.end=ffiproc_frame_end,
	.step=ffiproc_frame_step,
};

inline static void initFfiprocFrameContext(void* data[6],FklVMvalue* proc,FklVMgc* gc)
{
	FfiprocFrameContext* c=(FfiprocFrameContext*)data;
	fklSetRef(&c->proc,proc,gc);
	c->state=FFIPROC_READY;
}

static void _ffi_call_proc(FklVMvalue* ffiproc,FklVM* exe,FklVMvalue* rel)
{
	FklVMframe* prev=exe->frames;
	FklVMframe* f=fklCreateOtherObjVMframe(&FfiprocContextMethodTable,prev);
	initFfiprocFrameContext(f->u.o.data,ffiproc,exe->gc);
	fklPushVMframe(f,exe);
}

extern int _mem_equal(const FklVMudata* a,const FklVMudata* b);

static void _ffi_proc_atomic(void* data,FklVMgc* gc)
{
	FklFfiProc* proc=data;
	fklGC_toGrey(proc->pd,gc);
}

static FklVMudMethodTable FfiProcMethodTable=
{
	.__princ=_ffi_proc_print,
	.__prin1=_ffi_proc_print,
	.__finalizer=_ffi_proc_atomic_finalizer,
	.__equal=_mem_equal,
	.__call=_ffi_call_proc,
	.__write=NULL,
	.__atomic=_ffi_proc_atomic,
	.__append=NULL,
	.__copy=NULL,
	.__hash=NULL,
	.__setq_hook=NULL,
};

int fklFfiIsProc(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.ud->type==fklFfiGetFfiMemUdSid()&&p->u.ud->t==&FfiProcMethodTable;
}

int fklFfiIsValidFunctionTypeId(FklSid_t id,FklFfiPublicData* pd)
{
	FklDefFuncType* ft=(FklDefFuncType*)FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(id,pd).all);
	uint32_t anum=ft->anum;
	FklTypeId_t* atypes=ft->atypes;
	FklTypeId_t rtype=ft->rtype;
	uint32_t i=0;
	for(;i<anum;i++)
	{
		if(!fklFfiIsArrayTypeId(atypes[i],pd)&&fklFfiGetTypeSizeWithTypeId(atypes[i],pd)>sizeof(void*))
			return 0;
	}
	if(rtype&&!fklFfiIsArrayTypeId(rtype,pd)&&fklFfiGetTypeSizeWithTypeId(rtype,pd)>sizeof(void*))
		return 0;
	return 1;
}

FklFfiProc* fklFfiCreateProc(FklTypeId_t type,void* func,FklSid_t sid,FklVMvalue* pd)
{
	FklFfiProc* tmp=(FklFfiProc*)malloc(sizeof(FklFfiProc));
	FKL_ASSERT(tmp);
	tmp->type=type;
	tmp->func=func;
	tmp->sid=sid;
	tmp->pd=pd;
	FklFfiPublicData* publicData=pd->u.ud->data;
	FklDefFuncType* ft=(FklDefFuncType*)FKL_GET_TYPES_PTR(fklFfiGetTypeUnion(type,publicData).all);
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

FklVMudata* fklFfiCreateProcUd(FklTypeId_t type
		,const char* cStr
		,FklVMvalue* rel,FklVMvalue* pd)
{
	FklFfiPublicData* publicData=pd->u.ud->data;
	pthread_rwlock_rdlock(&publicData->sharedObjsLock);
	void* address=NULL;
	for(FklFfiSharedObjNode* head=publicData->sharedObjs;head;head=head->next)
	{
		address=fklGetAddress(cStr,head->dll);
		if(address)
			break;
	}
	pthread_rwlock_unlock(&publicData->sharedObjsLock);
	if(!address)
		return NULL;
	return fklCreateVMudata(fklFfiGetFfiMemUdSid(),&FfiProcMethodTable,fklFfiCreateProc(type,address,fklAddSymbolToGlobCstr(cStr)->id,pd),rel);
}
