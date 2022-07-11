#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
FklVMproc* fklNewVMproc(uint64_t scp,uint64_t cpc)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp,__func__);
	tmp->prevEnv=FKL_VM_NIL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	tmp->sid=0;
	return tmp;
}

FklVMvalue* fklCopyVMvalue(FklVMvalue* obj,FklVMstack* s,FklVMheap* heap)
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
			case FKL_NIL_TAG:
			case FKL_I32_TAG:
			case FKL_SYM_TAG:
			case FKL_CHR_TAG:
				*root1=root;
				break;
			case FKL_PTR_TAG:
				{
					FklValueType type=root->type;
					switch(type)
					{
						case FKL_PAIR:
							*root1=fklNewVMvalueToStack(FKL_PAIR,fklNewVMpair(),s,heap);
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

FklVMvalue* fklNewVMvalue(FklValueType type,void* pValue,FklVMheap* heap)
{
	FklVMvalue* r=fklNewSaveVMvalue(type,pValue);
	fklAddToHeap(r,heap);
	return r;
}

FklVMvalue* fklNewVMvalueToStack(FklValueType type
		,void* pValue
		,FklVMstack* stack
		,FklVMheap* heap)
{
	FklVMvalue* r=fklNewSaveVMvalue(type,pValue);
	pthread_rwlock_wrlock(&stack->lock);
	if(stack->tp>=stack->size)
	{
		stack->values=(FklVMvalue**)realloc(stack->values
				,sizeof(FklVMvalue*)*(stack->size+64));
		FKL_ASSERT(stack->values,__func__);
		stack->size+=64;
	}
	pthread_rwlock_unlock(&stack->lock);
	fklAddToHeap(r,heap);
	pthread_rwlock_wrlock(&stack->lock);
	stack->values[stack->tp]=r;
	stack->tp++;
	pthread_rwlock_unlock(&stack->lock);
	return stack->values[stack->tp-1];
}

FklVMvalue* fklNewSaveVMvalue(FklValueType type,void* pValue)
{
	switch(type)
	{
		case FKL_NIL:
			return FKL_VM_NIL;
			break;
		case FKL_CHR:
			return FKL_MAKE_VM_CHR(pValue);
			break;
		case FKL_I32:
			return FKL_MAKE_VM_I32(pValue);
			break;
		case FKL_SYM:
			return FKL_MAKE_VM_SYM(pValue);
			break;
		default:
			{
				FklVMvalue* tmp=(FklVMvalue*)malloc(sizeof(FklVMvalue));
				FKL_ASSERT(tmp,__func__);
				tmp->type=type;
				tmp->mark=FKL_MARK_W;
				switch(type)
				{
					case FKL_F64:
						if(pValue)
							tmp->u.f64=getF64FromByteCode(pValue);
						break;
					case FKL_I64:
						if(pValue)
							tmp->u.i64=getI64FromByteCode(pValue);
						break;
					case FKL_STR:
						tmp->u.str=pValue;break;
					case FKL_PAIR:
						tmp->u.pair=pValue;break;
					case FKL_PROC:
						tmp->u.proc=pValue;break;
					case FKL_CONT:
						tmp->u.cont=pValue;break;
					case FKL_CHAN:
						tmp->u.chan=pValue;break;
					case FKL_FP:
						tmp->u.fp=pValue;break;
					case FKL_DLL:
						tmp->u.dll=pValue;break;
					case FKL_DLPROC:
						tmp->u.dlproc=pValue;break;
					case FKL_ERR:
						tmp->u.err=pValue;break;
					case FKL_VECTOR:
						tmp->u.vec=pValue;break;
					case FKL_USERDATA:
						tmp->u.ud=pValue;break;
					case FKL_ENV:
						tmp->u.env=pValue;break;
					case FKL_BIG_INT:
						tmp->u.bigInt=pValue;break;
					case FKL_BOX:
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

void fklAddToHeap(FklVMvalue* v,FklVMheap* heap)
{
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(heap);
		if(running>FKL_GC_MARK_ROOT&&running<FKL_GC_SWEEPING)
			fklGC_toGray(v,heap);
		pthread_rwlock_wrlock(&heap->lock);
		v->next=heap->head;
		heap->head=v;
		heap->num+=1;
		pthread_rwlock_unlock(&heap->lock);
	}
}

FklVMvalue* fklNewTrueValue(FklVMheap* heap)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_I32,(FklVMptr)1,heap);
	return tmp;
}

FklVMvalue* fklNewNilValue(FklVMheap* heap)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_NIL,NULL,heap);
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
				case FKL_F64:
					r=(fabs(root1->u.f64-root2->u.f64)==0);
					break;
				case FKL_I64:
					r=(root1->u.i64-root2->u.i64)==0;
				case FKL_STR:
					r=!fklStringcmp(root1->u.str,root2->u.str);
					break;
				case FKL_PAIR:
					r=1;
					fklPushPtrStack(root1->u.pair->car,s1);
					fklPushPtrStack(root1->u.pair->cdr,s1);
					fklPushPtrStack(root2->u.pair->car,s2);
					fklPushPtrStack(root2->u.pair->cdr,s2);
					break;
				case FKL_VECTOR:
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
				case FKL_BIG_INT:
					r=!fklCmpBigInt(root1->u.bigInt,root2->u.bigInt);
					break;
				case FKL_USERDATA:
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
	if(FKL_GET_TAG(fir)==FKL_GET_TAG(sec)&&FKL_GET_TAG(fir)==FKL_I32_TAG)
		return fir==sec;
	else
	{
		double first=(FKL_GET_TAG(fir)==FKL_I32_TAG)?FKL_GET_I32(fir)
			:((FKL_IS_I64(fir))?fir->u.i64:fir->u.f64);
		double second=(FKL_GET_TAG(sec)==FKL_I32_TAG)?FKL_GET_I32(sec)
			:((FKL_IS_I64(sec))?sec->u.i64:sec->u.f64);
		return fabs(first-second)==0;
	}
}

FklVMpair* fklNewVMpair(void)
{
	FklVMpair* tmp=(FklVMpair*)malloc(sizeof(FklVMpair));
	FKL_ASSERT(tmp,__func__);
	tmp->car=FKL_VM_NIL;
	tmp->cdr=FKL_VM_NIL;
	return tmp;
}

FklVMvalue* fklNewVMpairV(FklVMvalue* car,FklVMvalue* cdr,FklVMstack* stack,FklVMheap* heap)
{
	FklVMvalue* pair=fklNewVMvalueToStack(FKL_PAIR,fklNewVMpair(),stack,heap);
	fklSetRef(&pair->u.pair->car,car,heap);
	fklSetRef(&pair->u.pair->cdr,cdr,heap);
	return pair;
}

FklVMvalue* fklNewVMvecV(size_t size,FklVMvalue** base,FklVMstack* stack,FklVMheap* heap)
{
	FklVMvalue* vec=fklNewVMvalueToStack(FKL_VECTOR,fklNewVMvec(size),stack,heap);
	if(base)
		for(size_t i=0;i<size;i++)
			fklSetRef(&vec->u.vec->base[i],base[i],heap);
	return vec;
}

FklVMchanl* fklNewVMchanl(int32_t maxSize)
{
	FklVMchanl* tmp=(FklVMchanl*)malloc(sizeof(FklVMchanl));
	FKL_ASSERT(tmp,__func__);
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

int32_t fklGetNumVMchanl(FklVMchanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=fklLengthPtrQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

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
	FKL_ASSERT(vfp,__func__);
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
	FKL_ASSERT(realDllName,__func__);
	sprintf(realDllName,"%s%s",dllName,filetype);
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
	void (*init)(FklSymbolTable*,FklVMvalue* rel)=fklGetAddress("_fklInit",handle);
	if(init)
		init(fklGetGlobSymbolTable(),rel);
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
	FKL_ASSERT(tmp,__func__);
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
	FKL_ASSERT(t,__func__);
	t->who=fklCopyString(who);
	t->type=type;
	t->message=fklCopyString(message);
	return t;
}

FklVMerror* fklNewVMerrorCstr(const char* who,FklSid_t type,const char* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t,__func__);
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
	FKL_ASSERT(tmp,__func__);
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
	FKL_ASSERT(tmp,__func__);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeVMsend(FklVMsend* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
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

FklVMenv* fklNewVMenv(FklVMvalue* prev,FklVMheap* h)
{
	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
	FKL_ASSERT(tmp,__func__);
	pthread_rwlock_init(&tmp->lock,NULL);
	tmp->num=0;
	tmp->list=NULL;
	tmp->prev=prev;
	fklSetRef(&tmp->prev,prev,h);
	return tmp;
}

void fklFreeVMenv(FklVMenv* obj)
{
	pthread_rwlock_wrlock(&obj->lock);
	for(uint32_t i=0;i<obj->num;i++)
		fklFreeVMenvNode(obj->list[i]);
	free(obj->list);
	pthread_rwlock_unlock(&obj->lock);
	pthread_rwlock_destroy(&obj->lock);
	free(obj);
}

FklVMenvNode* fklNewVMenvNode(FklVMvalue* value,int32_t id)
{
	FklVMenvNode* tmp=(FklVMenvNode*)malloc(sizeof(FklVMenvNode));
	FKL_ASSERT(tmp,__func__);
	tmp->id=id;
	tmp->value=value;
	return tmp;
}

FklVMenvNode* fklAddVMenvNode(FklSid_t id,FklVMenv* env)
{
	FklVMenvNode* r=NULL;
	pthread_rwlock_rdlock(&env->lock);
	if(!env->list)
	{
		env->num=1;
		env->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*1);
		FKL_ASSERT(env->list,__func__);
		r=fklNewVMenvNode(FKL_VM_NIL,id);
		env->list[0]=r;
	}
	else
	{
		int32_t l=0;
		int32_t h=env->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(env->list[mid]->id>id)
				h=mid-1;
			else if(env->list[mid]->id==id)
			{
				r=env->list[mid];
				pthread_rwlock_unlock(&env->lock);
				return r;
			}
			else
				l=mid+1;
		}
		pthread_rwlock_unlock(&env->lock);
		pthread_rwlock_wrlock(&env->lock);
		if(env->list[mid]->id<=id)
			mid++;
		env->num+=1;
		int32_t i=env->num-1;
		env->list=(FklVMenvNode**)realloc(env->list,sizeof(FklVMenvNode*)*env->num);
		FKL_ASSERT(env->list,__func__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		r=fklNewVMenvNode(FKL_VM_NIL,id);
		env->list[mid]=r;
	}
	pthread_rwlock_unlock(&env->lock);
	return r;
}

FklVMenvNode* fklAddVMenvNodeWithV(FklSid_t id,FklVMvalue* value,FklVMenv* env)
{
	FklVMenvNode* r=NULL;
	pthread_rwlock_wrlock(&env->lock);
	if(!env->list)
	{
		env->num=1;
		env->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*1);
		FKL_ASSERT(env->list,__func__);
		r=fklNewVMenvNode(value,id);
		env->list[0]=r;
	}
	else
	{
		int32_t l=0;
		int32_t h=env->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(env->list[mid]->id>id)
				h=mid-1;
			else if(env->list[mid]->id==id)
			{
				r=env->list[mid];
				pthread_rwlock_unlock(&env->lock);
				r->value=value;
				return r;
			}
			else
				l=mid+1;
		}
		if(env->list[mid]->id<=id)
			mid++;
		env->num+=1;
		int32_t i=env->num-1;
		env->list=(FklVMenvNode**)realloc(env->list,sizeof(FklVMenvNode*)*env->num);
		FKL_ASSERT(env->list,__func__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		r=fklNewVMenvNode(value,id);
		env->list[mid]=r;
	}
	pthread_rwlock_unlock(&env->lock);
	return r;
}

FklVMenvNode* fklFindVMenvNode(FklSid_t id,FklVMenv* env)
{
	if(!env->list)
		return NULL;
	pthread_rwlock_rdlock(&env->lock);
	int32_t l=0;
	int32_t h=env->num-1;
	int32_t mid;
	FklVMenvNode* r=NULL;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		if(env->list[mid]->id>id)
			h=mid-1;
		else if(env->list[mid]->id<id)
			l=mid+1;
		else
		{
			r=env->list[mid];
			break;
		}
	}
	pthread_rwlock_unlock(&env->lock);
	return r;
}

void fklFreeVMenvNode(FklVMenvNode* node)
{
	free(node);
}

FklVMvalue* fklCastCptrVMvalue(FklAstCptr* objCptr,FklVMheap* heap)
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
		if(root->type==FKL_ATM)
		{
			FklAstAtom* tmpAtm=root->u.atom;
			switch(tmpAtm->type)
			{
				case FKL_I32:
					*root1=FKL_MAKE_VM_I32(tmpAtm->value.i32);
					break;
				case FKL_CHR:
					*root1=FKL_MAKE_VM_CHR(tmpAtm->value.chr);
					break;
				case FKL_SYM:
					*root1=FKL_MAKE_VM_SYM(fklAddSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case FKL_F64:
					*root1=fklNewVMvalue(FKL_F64,&tmpAtm->value.f64,heap);
					break;
				case FKL_STR:
					*root1=fklNewVMvalue(FKL_STR,fklNewString(tmpAtm->value.str->size,tmpAtm->value.str->str),heap);
					break;
				case FKL_BOX:
					*root1=fklNewVMvalue(FKL_BOX,FKL_VM_NIL,heap);
					fklPushPtrStack(&tmpAtm->value.box,s1);
					fklPushPtrStack(&(*root1)->u.box,s2);
					break;
				case FKL_VECTOR:
					*root1=fklNewVMvalue(FKL_VECTOR,fklNewVMvec(tmpAtm->value.vec.size),heap);
					for(size_t i=0;i<tmpAtm->value.vec.size;i++)
					{
						fklPushPtrStack(&tmpAtm->value.vec.base[i],s1);
						fklPushPtrStack(&(*root1)->u.vec->base[i],s2);
					}
					break;
				case FKL_BIG_INT:
					{
						FklBigInt* bi=fklNewBigInt0();
						fklSetBigInt(bi,&tmpAtm->value.bigInt);
						*root1=fklNewVMvalue(FKL_BIG_INT,bi,heap);
					}
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==FKL_PAIR)
		{
			FklAstPair* objPair=root->u.pair;
			FklVMpair* tmpPair=fklNewVMpair();
			*root1=fklNewVMvalue(FKL_PAIR,tmpPair,heap);
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
	FKL_ASSERT(r,__func__);
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
	FKL_ASSERT(*fir,__func__);
	(*fir)->size=firSize+secSize;
	for(size_t i=0;i<secSize;i++)
		(*fir)->base[firSize+i]=sec->base[i];
}

FklVMudata* fklNewVMudata(FklSid_t type,FklVMudMethodTable* t,void* mem,FklVMvalue* rel)
{
	FklVMudata* r=(FklVMudata*)malloc(sizeof(FklVMudata));
	FKL_ASSERT(r,__func__);
	r->type=type;
	r->t=t;
	r->rel=rel;
	r->data=mem;
	return r;
}

int fklIsInvokableUd(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&v->u.ud->t->__invoke;
}

int fklIsInvokeable(FklVMvalue* v)
{
	return FKL_IS_PROC(v)||FKL_IS_DLPROC(v)||FKL_IS_CONT(v)||fklIsInvokableUd(v);
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
	FklVMvalue* nextInvoke=exe->nextInvoke;
	uint32_t i=0;
	FklVMcontinuation* tmp=(FklVMcontinuation*)malloc(sizeof(FklVMcontinuation));
	FKL_ASSERT(tmp,__func__);
	uint32_t tbnum=tstack->top;
	FklVMtryBlock* tb=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock)*tbnum);
	FKL_ASSERT(tb,__func__);
	tmp->stack=fklCopyStack(stack);
	tmp->stack->tp=ap;
	tmp->curr=NULL;
	tmp->nextInvoke=nextInvoke;
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
