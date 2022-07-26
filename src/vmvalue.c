#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
FklVMproc* fklNewVMproc(uint64_t scp,uint64_t cpc)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp);
	tmp->prevEnv=FKL_VM_NIL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	tmp->sid=0;
	return tmp;
}

FklVMvalue* fklCopyVMlistOrAtom(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklPtrStack* s2=fklNewPtrStack(32,16);
	FklVMvalue* tmp=FKL_VM_NIL;
	fklPushPtrStack(obj,s1);
	fklPushPtrStack(&tmp,s2);
	while(!fklIsPtrStackEmpty(s1))
	{
		FklVMvalue* root=fklPopPtrStack(s1);
		FklVMptrTag tag=FKL_GET_TAG(root);
		FklVMvalue** root1=fklPopPtrStack(s2);
		switch(tag)
		{
			case FKL_TAG_NIL:
			case FKL_TAG_I32:
			case FKL_TAG_SYM:
			case FKL_TAG_CHR:
				*root1=root;
				break;
			case FKL_TAG_PTR:
				{
					FklValueType type=root->type;
					switch(type)
					{
						case FKL_TYPE_PAIR:
							*root1=fklNewVMvalueToStack(FKL_TYPE_PAIR,fklNewVMpair(),s,gc);
							fklPushPtrStack(&(*root1)->u.pair->car,s2);
							fklPushPtrStack(&(*root1)->u.pair->cdr,s2);
							fklPushPtrStack(root->u.pair->car,s1);
							fklPushPtrStack(root->u.pair->cdr,s1);
							break;
						default:
							*root1=root;
							break;
					}
					*root1=FKL_MAKE_VM_PTR(*root1);
				}
				break;
			default:
				return NULL;
				break;
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return tmp;
}

static FklVMvalue* __fkl_f64_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	FklVMvalue* tmp=fklNewVMvalueToStack(FKL_TYPE_F64,NULL,s,gc);
	tmp->u.f64=obj->u.f64;
	return tmp;
}

static FklVMvalue* __fkl_i64_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	FklVMvalue* tmp=fklNewVMvalueToStack(FKL_TYPE_I64,NULL,s,gc);
	tmp->u.i64=obj->u.i64;
	return tmp;
}

static FklVMvalue* __fkl_bigint_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	return fklNewVMvalueToStack(FKL_TYPE_BIG_INT,fklCopyBigInt(obj->u.bigInt),s,gc);
}

static FklVMvalue* __fkl_vector_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	return fklNewVMvecV(obj->u.vec->size,obj->u.vec->base,s,gc);
}

static FklVMvalue* __fkl_str_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	return fklNewVMvalueToStack(FKL_TYPE_STR,fklCopyString(obj->u.str),s,gc);
}

static FklVMvalue* __fkl_bytevector_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	return fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCopyBytevector(obj->u.bvec),s,gc);
}

static FklVMvalue* __fkl_pair_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
//	return fklNewVMpairV(obj->u.pair->car,obj->u.pair->cdr,s,gc);
	return fklCopyVMlistOrAtom(obj,s,gc);
}

static FklVMvalue* __fkl_box_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	return fklNewVMvalueToStack(FKL_TYPE_BOX,obj->u.box,s,gc);
}

static FklVMvalue* __fkl_userdata_copyer(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	if(obj->u.ud->t->__copy==NULL)
		return NULL;
	else
		return fklNewVMvalueToStack(FKL_TYPE_USERDATA
				,fklNewVMudata(obj->u.ud->type
					,obj->u.ud->t
					,obj->u.ud->t->__copy(obj->u.ud->data)
					,obj->u.ud->rel)
				,s,gc);
}

static FklVMvalue* (*const valueCopyers[])(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)=
{
	NULL,
	NULL,
	NULL,
	NULL,
	__fkl_f64_copyer,
	__fkl_i64_copyer,
	__fkl_bigint_copyer,
	__fkl_str_copyer,
	__fkl_vector_copyer,
	__fkl_pair_copyer,
	__fkl_box_copyer,
	__fkl_bytevector_copyer,
	__fkl_userdata_copyer,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

FklVMvalue* fklCopyVMvalue(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)
{
	FklVMvalue* tmp=FKL_VM_NIL;
	FklVMptrTag tag=FKL_GET_TAG(obj);
	switch(tag)
	{
		case FKL_TAG_NIL:
		case FKL_TAG_I32:
		case FKL_TAG_SYM:
		case FKL_TAG_CHR:
			tmp=obj;
			break;
		case FKL_TAG_PTR:
			{
				FklValueType type=obj->type;
				FKL_ASSERT(type<FKL_TYPE_ATM);
				FklVMvalue* (*valueCopyer)(FklVMvalue* obj,FklVMstack* s,FklVMgc* gc)=valueCopyers[type];
				if(!valueCopyer)
					return NULL;
				else
					return valueCopyer(obj,s,gc);
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return tmp;
}

static inline double getF64FromByteCode(uint8_t* code)
{
	double i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	((uint8_t*)&i)[4]=code[4];
	((uint8_t*)&i)[5]=code[5];
	((uint8_t*)&i)[6]=code[6];
	((uint8_t*)&i)[7]=code[7];
	return i;
}

static inline int64_t getI64FromByteCode(uint8_t* code)
{
	int64_t i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	((uint8_t*)&i)[4]=code[4];
	((uint8_t*)&i)[5]=code[5];
	((uint8_t*)&i)[6]=code[6];
	((uint8_t*)&i)[7]=code[7];
	return i;
}

FklVMvalue* fklNewVMvalue(FklValueType type,void* pValue,FklVMgc* gc)
{
	FklVMvalue* r=fklNewSaveVMvalue(type,pValue);
	fklAddToGC(r,gc);
	return r;
}

FklVMvalue* fklNewVMvalueToStack(FklValueType type
		,void* pValue
		,FklVMstack* stack
		,FklVMgc* gc)
{
	FklVMvalue* r=fklNewSaveVMvalue(type,pValue);
//	pthread_rwlock_wrlock(&stack->lock);
	if(stack->tp>=stack->size)
	{
		stack->values=(FklVMvalue**)realloc(stack->values
				,sizeof(FklVMvalue*)*(stack->size+64));
		FKL_ASSERT(stack->values);
		stack->size+=64;
	}
	stack->values[stack->tp]=r;
	stack->tp++;
//	pthread_rwlock_unlock(&stack->lock);
	fklAddToGCBeforeGC(r,gc);
	return stack->values[stack->tp-1];
}

FklVMvalue* fklNewSaveVMvalue(FklValueType type,void* pValue)
{
	switch(type)
	{
		case FKL_TYPE_NIL:
			return FKL_VM_NIL;
			break;
		case FKL_TYPE_CHR:
			return FKL_MAKE_VM_CHR(pValue);
			break;
		case FKL_TYPE_I32:
			return FKL_MAKE_VM_I32(pValue);
			break;
		case FKL_TYPE_SYM:
			return FKL_MAKE_VM_SYM(pValue);
			break;
		default:
			{
				FklVMvalue* tmp=(FklVMvalue*)malloc(sizeof(FklVMvalue));
				FKL_ASSERT(tmp);
				tmp->type=type;
				tmp->mark=FKL_MARK_W;
				switch(type)
				{
					case FKL_TYPE_F64:
						if(pValue)
							tmp->u.f64=getF64FromByteCode(pValue);
						break;
					case FKL_TYPE_I64:
						if(pValue)
							tmp->u.i64=getI64FromByteCode(pValue);
						break;
					case FKL_TYPE_BYTEVECTOR:
						tmp->u.bvec=pValue;break;
					case FKL_TYPE_STR:
						tmp->u.str=pValue;break;
					case FKL_TYPE_PAIR:
						tmp->u.pair=pValue;break;
					case FKL_TYPE_PROC:
						tmp->u.proc=pValue;break;
					case FKL_TYPE_CONT:
						tmp->u.cont=pValue;break;
					case FKL_TYPE_CHAN:
						tmp->u.chan=pValue;break;
					case FKL_TYPE_FP:
						tmp->u.fp=pValue;break;
					case FKL_TYPE_DLL:
						tmp->u.dll=pValue;break;
					case FKL_TYPE_DLPROC:
						tmp->u.dlproc=pValue;break;
					case FKL_TYPE_ERR:
						tmp->u.err=pValue;break;
					case FKL_TYPE_VECTOR:
						tmp->u.vec=pValue;break;
					case FKL_TYPE_USERDATA:
						tmp->u.ud=pValue;break;
					case FKL_TYPE_ENV:
						tmp->u.env=pValue;break;
					case FKL_TYPE_BIG_INT:
						tmp->u.bigInt=pValue;break;
					case FKL_TYPE_BOX:
						tmp->u.box=pValue;break;
					default:
						return NULL;
						break;
				}
				return FKL_MAKE_VM_PTR(tmp);
			}
			break;
	}
}

void fklAddToGCNoGC(FklVMvalue* v,FklVMgc* gc)
{
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(gc);
		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
			fklGC_toGrey(v,gc);
		pthread_rwlock_wrlock(&gc->lock);
		v->next=gc->head;
		gc->head=v;
		gc->num+=1;
		pthread_rwlock_unlock(&gc->lock);
	}
}

static void tryGC(FklVMgc* gc)
{
	if(gc->num>gc->threshold)
		fklGC_threadFunc(gc);
}

FklVMvalue* fklNewVMvalueNoGC(FklValueType type,void* pValue,FklVMgc* gc)
{
	FklVMvalue* r=fklNewSaveVMvalue(type,pValue);
	fklAddToGCNoGC(r,gc);
	return r;
}

void fklAddToGCBeforeGC(FklVMvalue* v,FklVMgc* gc)
{
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(gc);
		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
			fklGC_toGrey(v,gc);
		pthread_rwlock_wrlock(&gc->lock);
		gc->num+=1;
		v->next=gc->head;
		gc->head=v;
		tryGC(gc);
		pthread_rwlock_unlock(&gc->lock);
	}
}

void fklAddToGC(FklVMvalue* v,FklVMgc* gc)
{
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(gc);
		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
			fklGC_toGrey(v,gc);
		pthread_rwlock_wrlock(&gc->lock);
		gc->num+=1;
		tryGC(gc);
		v->next=gc->head;
		gc->head=v;
		pthread_rwlock_unlock(&gc->lock);
	}
}

FklVMvalue* fklNewTrueValue(FklVMgc* gc)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_TYPE_I32,(FklVMptr)1,gc);
	return tmp;
}

FklVMvalue* fklNewNilValue(FklVMgc* gc)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_TYPE_NIL,NULL,gc);
	return tmp;
}

FklVMvalue* fklGetVMpairCar(FklVMvalue* obj)
{
	return obj->u.pair->car;
}

FklVMvalue* fklGetVMpairCdr(FklVMvalue* obj)
{
	return obj->u.pair->cdr;
}

int fklVMvaluecmp(FklVMvalue* fir,FklVMvalue* sec)
{
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklPtrStack* s2=fklNewPtrStack(32,16);
	fklPushPtrStack(fir,s1);
	fklPushPtrStack(sec,s2);
	int r=1;
	while(!fklIsPtrStackEmpty(s1))
	{
		FklVMvalue* root1=fklPopPtrStack(s1);
		FklVMvalue* root2=fklPopPtrStack(s2);
		if(!FKL_IS_PTR(root1)&&!FKL_IS_PTR(root2)&&root1!=root2)
			r=0;
		else if(FKL_GET_TAG(root1)!=FKL_GET_TAG(root2))
			r=0;
		else if(FKL_IS_PTR(root1)&&FKL_IS_PTR(root2))
		{
			if(root1->type!=root2->type)
				r=0;
			switch(root1->type)
			{
				case FKL_TYPE_F64:
					r=root1->u.f64==root2->u.f64;
					break;
				case FKL_TYPE_I64:
					r=(root1->u.i64-root2->u.i64)==0;
				case FKL_TYPE_STR:
					r=!fklStringcmp(root1->u.str,root2->u.str);
					break;
				case FKL_TYPE_BYTEVECTOR:
					r=!fklBytevectorcmp(root1->u.bvec,root2->u.bvec);
					break;
				case FKL_TYPE_PAIR:
					r=1;
					fklPushPtrStack(root1->u.pair->car,s1);
					fklPushPtrStack(root1->u.pair->cdr,s1);
					fklPushPtrStack(root2->u.pair->car,s2);
					fklPushPtrStack(root2->u.pair->cdr,s2);
					break;
				case FKL_TYPE_BOX:
					r=1;
					fklPushPtrStack(root1->u.box,s1);
					fklPushPtrStack(root2->u.box,s1);
					break;
				case FKL_TYPE_VECTOR:
					if(root1->u.vec->size!=root2->u.vec->size)
						r=0;
					else
					{
						r=1;
						size_t size=root1->u.vec->size;
						for(size_t i=0;i<size;i++)
							fklPushPtrStack(root1->u.vec->base[i],s1);
						for(size_t i=0;i<size;i++)
							fklPushPtrStack(root2->u.vec->base[i],s2);
					}
					break;
				case FKL_TYPE_BIG_INT:
					r=!fklCmpBigInt(root1->u.bigInt,root2->u.bigInt);
					break;
				case FKL_TYPE_USERDATA:
					if(root1->u.ud->type!=root2->u.ud->type||!root1->u.ud->t->__equal)
						r=0;
					else
						r=root1->u.ud->t->__equal(root1->u.ud,root2->u.ud);
					break;
				default:
					r=(root1==root2);
					break;
			}
		}
		if(!r)
			break;
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return r;
}

int fklNumcmp(FklVMvalue* fir,FklVMvalue* sec)
{
	if(FKL_GET_TAG(fir)==FKL_GET_TAG(sec)&&FKL_GET_TAG(fir)==FKL_TAG_I32)
		return fir==sec;
	else
	{
		double first=(FKL_GET_TAG(fir)==FKL_TAG_I32)?FKL_GET_I32(fir)
			:((FKL_IS_I64(fir))?fir->u.i64:fir->u.f64);
		double second=(FKL_GET_TAG(sec)==FKL_TAG_I32)?FKL_GET_I32(sec)
			:((FKL_IS_I64(sec))?sec->u.i64:sec->u.f64);
		return fabs(first-second)==0;
	}
}

FklVMpair* fklNewVMpair(void)
{
	FklVMpair* tmp=(FklVMpair*)malloc(sizeof(FklVMpair));
	FKL_ASSERT(tmp);
	tmp->car=FKL_VM_NIL;
	tmp->cdr=FKL_VM_NIL;
	return tmp;
}

FklVMvalue* fklNewVMpairV(FklVMvalue* car,FklVMvalue* cdr,FklVMstack* stack,FklVMgc* gc)
{
	FklVMvalue* pair=fklNewVMvalueToStack(FKL_TYPE_PAIR,fklNewVMpair(),stack,gc);
	fklSetRef(&pair->u.pair->car,car,gc);
	fklSetRef(&pair->u.pair->cdr,cdr,gc);
	return pair;
}

FklVMvalue* fklNewVMvecV(size_t size,FklVMvalue** base,FklVMstack* stack,FklVMgc* gc)
{
	FklVMvalue* vec=fklNewVMvalueToStack(FKL_TYPE_VECTOR,fklNewVMvec(size),stack,gc);
	if(base)
		for(size_t i=0;i<size;i++)
			fklSetRef(&vec->u.vec->base[i],base[i],gc);
	return vec;
}

FklVMchanl* fklNewVMchanl(int32_t maxSize)
{
	FklVMchanl* tmp=(FklVMchanl*)malloc(sizeof(FklVMchanl));
	FKL_ASSERT(tmp);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->messageNum=0;
	tmp->sendNum=0;
	tmp->recvNum=0;
	tmp->messages=fklNewPtrQueue();
	tmp->sendq=fklNewPtrQueue();
	tmp->recvq=fklNewPtrQueue();
	return tmp;
}

//int32_t fklGetNumVMchanl(FklVMchanl* ch)
//{
//	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
//	uint32_t i=0;
//	i=fklLengthPtrQueue(ch->messages);
//	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
//	return i;
//}

void fklFreeVMchanl(FklVMchanl* ch)
{
	pthread_mutex_destroy(&ch->lock);
	fklFreePtrQueue(ch->messages);
	FklQueueNode* head=ch->sendq->head;
	for(;head;head=head->next)
		fklFreeVMsend(head->data);
	for(head=ch->recvq->head;head;head=head->next)
		fklFreeVMrecv(head->data);
	fklFreePtrQueue(ch->sendq);
	fklFreePtrQueue(ch->recvq);
	free(ch);
}

void fklFreeVMproc(FklVMproc* proc)
{
	free(proc);
}

FklVMfp* fklNewVMfp(FILE* fp)
{
	FklVMfp* vfp=(FklVMfp*)malloc(sizeof(FklVMfp));
	FKL_ASSERT(vfp);
	vfp->fp=fp;
	vfp->size=0;
	vfp->prev=NULL;
	return vfp;
}

int fklFreeVMfp(FklVMfp* vfp)
{
	int r=0;
	if(vfp)
	{
		if(vfp->size)
			free(vfp->prev);
		FILE* fp=vfp->fp;
		if(!(fp!=NULL&&fp!=stdin&&fp!=stdout&&fp!=stderr&&fclose(fp)!=EOF))
			r=1;
		free(vfp);
	}
	return r;
}

FklVMdllHandle fklLoadDll(const char* path)
{
#ifdef _WIN32
	FklVMdllHandle handle=LoadLibraryExA(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	FklVMdllHandle handle=dlopen(path,RTLD_LAZY);
#endif
	return handle;
}

FklVMdllHandle fklNewVMdll(const char* dllName)
{
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	size_t len=strlen(dllName)+strlen(filetype)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDllName);
	strcpy(realDllName,dllName);
	strcat(realDllName,filetype);
	char* rpath=fklRealpath(realDllName);
	if(!rpath)
	{
		free(realDllName);
		return NULL;
	}
	FklVMdllHandle handle=fklLoadDll(rpath);
	if(!handle)
	{
		putc('\n',stderr);
		free(rpath);
		free(realDllName);
		return NULL;
	}
	free(realDllName);
	free(rpath);
	return handle;
}

void fklInitVMdll(FklVMvalue* rel)
{
	FklVMdllHandle handle=rel->u.dll;
	void (*init)(FklSymbolTable*,FklVMvalue* rel,FklVMlist*)=fklGetAddress("_fklInit",handle);
	if(init)
		init(fklGetGlobSymbolTable(),rel,fklGetGlobVMs());
}

void fklFreeVMvalue(FklVMvalue* cur)
{
	switch(cur->type)
	{
		case FKL_TYPE_STR:
			free(cur->u.str);
			break;
		case FKL_TYPE_BYTEVECTOR:
			free(cur->u.bvec);
			break;
		case FKL_TYPE_PAIR:
			free(cur->u.pair);
			break;
		case FKL_TYPE_PROC:
			fklFreeVMproc(cur->u.proc);
			break;
		case FKL_TYPE_CONT:
			fklFreeVMcontinuation(cur->u.cont);
			break;
		case FKL_TYPE_CHAN:
			fklFreeVMchanl(cur->u.chan);
			break;
		case FKL_TYPE_FP:
			fklFreeVMfp(cur->u.fp);
			break;
		case FKL_TYPE_DLL:
			fklFreeVMdll(cur->u.dll);
			break;
		case FKL_TYPE_DLPROC:
			fklFreeVMdlproc(cur->u.dlproc);
			break;
		case FKL_TYPE_ERR:
			fklFreeVMerror(cur->u.err);
			break;
		case FKL_TYPE_VECTOR:
			fklFreeVMvec(cur->u.vec);
			break;
		case FKL_TYPE_USERDATA:
			if(cur->u.ud->t->__finalizer)
				cur->u.ud->t->__finalizer(cur->u.ud->data);
			fklFreeVMudata(cur->u.ud);
			break;
		case FKL_TYPE_F64:
		case FKL_TYPE_I64:
		case FKL_TYPE_BOX:
			break;
		case FKL_TYPE_ENV:
			fklFreeVMenv(cur->u.env);
			break;
		case FKL_TYPE_BIG_INT:
			fklFreeBigInt(cur->u.bigInt);
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	free((void*)cur);
}

void fklFreeVMdll(FklVMdllHandle dll)
{
	if(dll)
	{
		void (*uninit)(void)=fklGetAddress("_fklUninit",dll);
		if(uninit)
			uninit();
#ifdef _WIN32
		FreeLibrary(dll);
#else
		dlclose(dll);
#endif
	}
}

void* fklGetAddress(const char* funcname,FklVMdllHandle dlhandle)
{
	void* pfunc=NULL;
#ifdef _WIN32
		pfunc=GetProcAddress(dlhandle,funcname);
#else
		pfunc=dlsym(dlhandle,funcname);
#endif
	return pfunc;
}

FklVMdlproc* fklNewVMdlproc(FklVMdllFunc address,FklVMvalue* dll)
{
	FklVMdlproc* tmp=(FklVMdlproc*)malloc(sizeof(FklVMdlproc));
	FKL_ASSERT(tmp);
	tmp->func=address;
	tmp->dll=dll;
	tmp->sid=0;
	return tmp;
}

void fklFreeVMdlproc(FklVMdlproc* dlproc)
{
	free(dlproc);
}

FklVMerror* fklNewVMerror(const FklString* who,FklSid_t type,const FklString* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t);
	t->who=fklCopyString(who);
	t->type=type;
	t->message=fklCopyString(message);
	return t;
}

FklVMerror* fklNewVMerrorCstr(const char* who,FklSid_t type,const char* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t);
	t->who=fklNewStringFromCstr(who);
	t->type=type;
	t->message=fklNewStringFromCstr(message);
	return t;
}

void fklFreeVMerror(FklVMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

FklVMrecv* fklNewVMrecv(void)
{
	FklVMrecv* tmp=(FklVMrecv*)malloc(sizeof(FklVMrecv));
	FKL_ASSERT(tmp);
	tmp->v=NULL;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeVMrecv(FklVMrecv* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

FklVMsend* fklNewVMsend(FklVMvalue* m)
{
	FklVMsend* tmp=(FklVMsend*)malloc(sizeof(FklVMsend));
	FKL_ASSERT(tmp);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeVMsend(FklVMsend* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

void fklChanlRecvOk(FklVMchanl* ch,FklVMvalue** r,int* ok)
{
	if(ch->messageNum)
	{
		pthread_mutex_lock(&ch->lock);
		*r=fklPopPtrQueue(ch->messages);
		ch->messageNum--;
		if(ch->messageNum<ch->max)
		{
			FklVMsend* s=fklPopPtrQueue(ch->sendq);
			ch->sendNum--;
			if(s)
				pthread_cond_signal(&s->cond);
		}
		pthread_mutex_unlock(&ch->lock);
		*ok=1;
	}
	else
		*ok=0;
}

void fklChanlRecv(FklVMrecv* r,FklVMchanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(!ch->messageNum)
	{
		fklPushPtrQueue(r,ch->recvq);
		ch->recvNum++;
		pthread_cond_wait(&r->cond,&ch->lock);
	}
	r->v=fklPopPtrQueue(ch->messages);
	ch->messageNum--;
	if(ch->messageNum<ch->max)
	{
		FklVMsend* s=fklPopPtrQueue(ch->sendq);
		ch->sendNum--;
		if(s)
			pthread_cond_signal(&s->cond);
	}
	pthread_mutex_unlock(&ch->lock);
}

void fklChanlSend(FklVMsend*s,FklVMchanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(ch->recvNum)
	{
		FklVMrecv* r=fklPopPtrQueue(ch->recvq);
		ch->recvNum--;
		if(r)
			pthread_cond_signal(&r->cond);
	}
	if(!ch->max||ch->messageNum<ch->max-1)
	{
		fklPushPtrQueue(s->m,ch->messages);
		ch->messageNum++;
	}
	else
	{
		if(ch->messageNum>=ch->max-1)
		{
			fklPushPtrQueue(s,ch->sendq);
			ch->sendNum++;
			if(ch->messageNum==ch->max-1)
			{
				fklPushPtrQueue(s->m,ch->messages);
				ch->messageNum++;
			}
			pthread_cond_wait(&s->cond,&ch->lock);
		}
	}
	fklFreeVMsend(s);
	pthread_mutex_unlock(&ch->lock);
}

typedef struct
{
	FklSid_t key;
	FklVMvalue* volatile v;
}VMenvHashItem;

VMenvHashItem* newVMenvHashItme(FklSid_t id,FklVMvalue* v)
{
	VMenvHashItem* r=(VMenvHashItem*)malloc(sizeof(VMenvHashItem));
	FKL_ASSERT(r);
	r->key=id;
	r->v=v;
	return r;
}

size_t _vmenv_hashFunc(void* key,FklHashTable* table)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid%table->size;
}

void _vmenv_freeItem(void* item)
{
	free(item);
}

int _vmenv_keyEqual(void* pkey0,void* pkey1)
{
	FklSid_t k0=*(FklSid_t*)pkey0;
	FklSid_t k1=*(FklSid_t*)pkey1;
	return k0==k1;
}

void* _vmenv_getKey(void* item)
{
	return &((VMenvHashItem*)item)->key;
}

static FklHashTableMethodTable VMenvHashMethTable=
{
	.__hashFunc=_vmenv_hashFunc,
	.__freeItem=_vmenv_freeItem,
	.__keyEqual=_vmenv_keyEqual,
	.__getKey=_vmenv_getKey,
};

FklVMenv* fklNewGlobVMenv(FklVMvalue* prev,FklVMgc* gc)
{
	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
	FKL_ASSERT(tmp);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->prev=prev;
	tmp->t=fklNewHashTable(512,4,2,0.75,1,&VMenvHashMethTable);
	fklSetRef(&tmp->prev,prev,gc);
	fklInitGlobEnv(tmp,gc);
	return tmp;
}

FklVMenv* fklNewVMenv(FklVMvalue* prev,FklVMgc* gc)
{
	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
	FKL_ASSERT(tmp);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->prev=prev;
	tmp->t=fklNewHashTable(8,4,2,0.75,1,&VMenvHashMethTable);
	fklSetRef(&tmp->prev,prev,gc);
	return tmp;
}

void fklAtomicVMenv(FklVMenv* env,FklVMgc* gc)
{
	FklHashTable* table=env->t;
	if(env->prev)
		fklGC_toGrey(env->prev,gc);
	for(size_t i=0;i<table->num;i++)
	{
		VMenvHashItem* item=table->list[i]->item;
		fklGC_toGrey(item->v,gc);
	}
}

FklVMvalue* volatile* fklFindVar(FklSid_t id,FklVMenv* env)
{
	FklVMvalue* volatile* r=NULL;
	VMenvHashItem* item=fklGetHashItem(&id,env->t);
	if(item)
		r=&item->v;
	return r;
}

FklVMvalue* volatile* fklFindOrAddVar(FklSid_t id,FklVMenv* env)
{
	FklVMvalue* volatile* r=NULL;
	r=fklFindVar(id,env);
	if(!r)
	{
		pthread_mutex_lock(&env->lock);
		VMenvHashItem* ritem=fklPutReplHashItem(newVMenvHashItme(id,FKL_VM_NIL),env->t);
		r=&ritem->v;
		pthread_mutex_unlock(&env->lock);
	}
	return r;
}

FklVMvalue* volatile* fklFindOrAddVarWithValue(FklSid_t id,FklVMvalue* v,FklVMenv* env)
{
	FklVMvalue* volatile* r=NULL;
	r=fklFindVar(id,env);
	if(!r)
	{
		pthread_mutex_lock(&env->lock);
		VMenvHashItem* ritem=fklPutReplHashItem(newVMenvHashItme(id,FKL_VM_NIL),env->t);
		r=&ritem->v;
		pthread_mutex_unlock(&env->lock);
	}
	*r=v;
	return r;
}

void fklDBG_printVMenv(FklVMenv* curEnv,FILE* fp)
{
	if(curEnv->t->num==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->t->num;i++)
		{
			VMenvHashItem* item=curEnv->t->list[i]->item;
			FklVMvalue* tmp=item->v;
			fklPrin1VMvalue(tmp,fp);
			putc(' ',fp);
		}
	}
}

void fklFreeVMenv(FklVMenv* obj)
{
	pthread_mutex_lock(&obj->lock);
	fklFreeHashTable(obj->t);
	pthread_mutex_unlock(&obj->lock);
	pthread_mutex_destroy(&obj->lock);
	free(obj);
}

FklVMvalue* fklCastCptrVMvalue(FklAstCptr* objCptr,FklVMgc* gc)
{
	FklPtrStack* s1=fklNewPtrStack(32,16);
	FklPtrStack* s2=fklNewPtrStack(32,16);
	FklVMvalue* tmp=FKL_VM_NIL;
	fklPushPtrStack(objCptr,s1);
	fklPushPtrStack(&tmp,s2);
	while(!fklIsPtrStackEmpty(s1))
	{
		FklAstCptr* root=fklPopPtrStack(s1);
		FklVMvalue** root1=fklPopPtrStack(s2);
		if(root->type==FKL_TYPE_ATM)
		{
			FklAstAtom* tmpAtm=root->u.atom;
			switch(tmpAtm->type)
			{
				case FKL_TYPE_I32:
					*root1=FKL_MAKE_VM_I32(tmpAtm->value.i32);
					break;
				case FKL_TYPE_CHR:
					*root1=FKL_MAKE_VM_CHR(tmpAtm->value.chr);
					break;
				case FKL_TYPE_SYM:
					*root1=FKL_MAKE_VM_SYM(fklAddSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case FKL_TYPE_F64:
					*root1=fklNewVMvalueNoGC(FKL_TYPE_F64,&tmpAtm->value.f64,gc);
					break;
				case FKL_TYPE_STR:
					*root1=fklNewVMvalueNoGC(FKL_TYPE_STR,fklNewString(tmpAtm->value.str->size,tmpAtm->value.str->str),gc);
					break;
				case FKL_TYPE_BYTEVECTOR:
					*root1=fklNewVMvalueNoGC(FKL_TYPE_BYTEVECTOR,fklNewBytevector(tmpAtm->value.bvec->size,tmpAtm->value.bvec->ptr),gc);
					break;
				case FKL_TYPE_BOX:
					*root1=fklNewVMvalueNoGC(FKL_TYPE_BOX,FKL_VM_NIL,gc);
					fklPushPtrStack(&tmpAtm->value.box,s1);
					fklPushPtrStack(&(*root1)->u.box,s2);
					break;
				case FKL_TYPE_VECTOR:
					*root1=fklNewVMvalueNoGC(FKL_TYPE_VECTOR,fklNewVMvec(tmpAtm->value.vec.size),gc);
					for(size_t i=0;i<tmpAtm->value.vec.size;i++)
					{
						fklPushPtrStack(&tmpAtm->value.vec.base[i],s1);
						fklPushPtrStack(&(*root1)->u.vec->base[i],s2);
					}
					break;
				case FKL_TYPE_BIG_INT:
					{
						FklBigInt* bi=fklNewBigInt0();
						fklSetBigInt(bi,&tmpAtm->value.bigInt);
						*root1=fklNewVMvalueNoGC(FKL_TYPE_BIG_INT,bi,gc);
					}
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==FKL_TYPE_PAIR)
		{
			FklAstPair* objPair=root->u.pair;
			FklVMpair* tmpPair=fklNewVMpair();
			*root1=fklNewVMvalueNoGC(FKL_TYPE_PAIR,tmpPair,gc);
			fklPushPtrStack(&objPair->car,s1);
			fklPushPtrStack(&objPair->cdr,s1);
			fklPushPtrStack(&tmpPair->car,s2);
			fklPushPtrStack(&tmpPair->cdr,s2);
			*root1=FKL_MAKE_VM_PTR(*root1);
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return tmp;
}

FklVMvec* fklNewVMvec(size_t size)
{
	FklVMvec* r=(FklVMvec*)malloc(sizeof(FklVMvec)+sizeof(FklVMvalue*)*size);
	FKL_ASSERT(r);
	r->size=size;
	for(size_t i=0;i<size;i++)
		r->base[i]=FKL_VM_NIL;
	return r;
}

void fklFreeVMvec(FklVMvec* vec)
{
	free(vec);
}

void fklVMvecCat(FklVMvec** fir,const FklVMvec* sec)
{
	size_t firSize=(*fir)->size;
	size_t secSize=sec->size;
	*fir=(FklVMvec*)realloc(*fir,sizeof(FklVMvec)+(firSize+secSize)*sizeof(FklVMvalue*));
	FKL_ASSERT(*fir);
	(*fir)->size=firSize+secSize;
	for(size_t i=0;i<secSize;i++)
		(*fir)->base[firSize+i]=sec->base[i];
}

FklVMudata* fklNewVMudata(FklSid_t type,FklVMudMethodTable* t,void* mem,FklVMvalue* rel)
{
	FklVMudata* r=(FklVMudata*)malloc(sizeof(FklVMudata));
	FKL_ASSERT(r);
	r->type=type;
	r->t=t;
	r->rel=rel;
	r->data=mem;
	return r;
}

int fklIsCallableUd(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&v->u.ud->t->__call;
}

int fklIsCallable(FklVMvalue* v)
{
	return FKL_IS_PROC(v)||FKL_IS_DLPROC(v)||FKL_IS_CONT(v)||fklIsCallableUd(v);
}

int fklIsAppendable(FklVMvalue* v)
{
	return FKL_IS_STR(v)
		||FKL_IS_BYTEVECTOR(v)
		||FKL_IS_VECTOR(v)
		||(FKL_IS_USERDATA(v)&&v->u.ud->t->__copy&&v->u.ud->t->__append);
}

void fklFreeVMudata(FklVMudata* u)
{
	free(u);
}

FklVMcontinuation* fklNewVMcontinuation(uint32_t ap,FklVM* exe)
{
	if(exe->nny)
		return NULL;
	FklVMstack* stack=exe->stack;
	FklVMrunnable* curr=exe->rhead;
	FklPtrStack* tstack=exe->tstack;
	FklVMvalue* nextCall=exe->nextCall;
	uint32_t i=0;
	FklVMcontinuation* tmp=(FklVMcontinuation*)malloc(sizeof(FklVMcontinuation));
	FKL_ASSERT(tmp);
	uint32_t tbnum=tstack->top;
	FklVMtryBlock* tb=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock)*tbnum);
	FKL_ASSERT(tb);
	tmp->stack=fklCopyStack(stack);
	tmp->stack->tp=ap;
	tmp->curr=NULL;
	tmp->nextCall=nextCall;
	for(FklVMrunnable* cur=curr;cur;cur=cur->prev)
	{
		FklVMrunnable* t=fklNewVMrunnable(NULL,tmp->curr);
		tmp->curr=t;
		t->cp=cur->cp;
		t->localenv=cur->localenv;
		t->cpc=cur->cpc;
		t->scp=cur->scp;
		t->sid=cur->sid;
		t->mark=cur->mark;
		t->ccc=fklCopyVMcCC(cur->ccc);
	}
	tmp->tnum=tbnum;
	for(i=0;i<tbnum;i++)
	{
		FklVMtryBlock* cur=tstack->base[i];
		tb[i].sid=cur->sid;
		FklPtrStack* hstack=cur->hstack;
		int32_t handlerNum=hstack->top;
		FklPtrStack* curHstack=fklNewPtrStack(handlerNum,handlerNum/2);
		int32_t j=0;
		for(;j<handlerNum;j++)
		{
			FklVMerrorHandler* curH=hstack->base[i];
			FklVMerrorHandler* h=fklNewVMerrorHandler(fklCopyMemory(curH->typeIds,sizeof(FklSid_t)*curH->num),curH->num,curH->proc.scp,curH->proc.cpc);
			fklPushPtrStack(h,curHstack);
		}
		tb[i].hstack=curHstack;
		tb[i].curr=cur->curr;
		tb[i].tp=cur->tp;
	}
	tmp->tb=tb;
	return tmp;
}

void fklFreeVMcontinuation(FklVMcontinuation* cont)
{
	int32_t i=0;
	int32_t tbsize=cont->tnum;
	FklVMstack* stack=cont->stack;
	FklVMrunnable* curr=cont->curr;
	while(curr)
	{
		FklVMrunnable* cur=curr;
		curr=curr->prev;
		fklFreeVMrunnable(cur);
	}
	FklVMtryBlock* tb=cont->tb;
	fklFreeUintStack(stack->tps);
	fklFreeUintStack(stack->bps);
	free(stack->values);
	free(stack);
	for(i=0;i<tbsize;i++)
	{
		FklPtrStack* hstack=tb[i].hstack;
		while(!fklIsPtrStackEmpty(hstack))
		{
			FklVMerrorHandler* h=fklPopPtrStack(hstack);
			fklFreeVMerrorHandler(h);
		}
		fklFreePtrStack(hstack);
	}
	free(tb);
	free(cont);
}
